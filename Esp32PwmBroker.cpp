/**
 * @file Esp32PwmBroker.cpp
 * @brief Implementation of the ESP32 PWM Broker.
 * 
 * @details Handles the physical allocation of LEDC resources. This implementation
 * uses a Meyers Singleton to ensure a single point of control for hardware 
 * registers, preventing resource conflicts between multiple motor instances.
 */
#include "Esp32PwmBroker.h"
#include "Esp32PwmControl.h"



/**
 * @details This is the entry point for the generic PwmBroker interface. 
 * It ensures that a call through the parent class is relayed to the 
 * specialized ESP32 broker to manage physical hardware.
 */

PwmBroker& PwmBroker::getInstance() {
	return Esp32PwmBroker::getInstance();
}



/**
 * @details Implements the Meyers Singleton pattern.
 * The instance is created only on the first call and is 
 * guaranteed to be thread-safe. This unique instance holds the 
 * registry for all 16 LEDC channels and 4 timers of the ESP32.
 */

Esp32PwmBroker& Esp32PwmBroker::getInstance() {
	static Esp32PwmBroker instance;
	return instance;
}



/**
 * @details This is the core logic of the Broker's leasing system. 
 * It performs a three-step orchestration to provide a hardware resource:
 * 
 * 1. Resource Search: It scans the 16 LEDC channels to find an available slot.
 * 2. Timer Optimization: It checks the 4 hardware timers. If a timer is already 
 *    running at the requested frequency, it shares it by incrementing its 
 *    client counter. Otherwise, it allocates a new timer.
 * 3. Automatic Resolution: For new timers, it runs a discovery routine 
 *    (using silent logging) to find the maximum possible bit resolution 
 *    for the given frequency.
 * 
 * @return A std::unique_ptr to a PwmControl object. If the allocation 
 * fails at any step, it returns a nullptr safely, preventing motor startup.
 */

std::unique_ptr<PwmControl> Esp32PwmBroker::requestResource(uint8_t pin, uint32_t freq) {

		// --- 1. Find a free channel ---
	int8_t channel = -1;
	for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++) {
		if (!_PwmChannelsInUse[i]) {
			channel = i;
			break;
		}
	}
	if (channel == -1) return nullptr; 

		// --- 2. Find or allocate a timer ---
	int8_t timer = allocateTimer(freq);
	if (timer == -1) return nullptr;

    // add client to timer lease counter
  _PwmTimers[timer].clients++;

		// --- 3. Hardware configuration (LEDC Channel only) ---
	ledc_channel_config_t config = {
		.gpio_num   = pin,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel    = (ledc_channel_t)channel,
		.intr_type  = LEDC_INTR_DISABLE,
		.timer_sel  = (ledc_timer_t)timer,
		.duty       = 0,
		.hpoint     = 0
	};
	
    // release timer and channel if configuration fails
	if (ledc_channel_config(&config) != ESP_OK) {
		releaseResource(channel, timer); 
		return nullptr;
	}

		// --- 4. Update channel and timer clients lease data ---
	_PwmChannelsInUse[channel] = true;
	
	uint32_t maxDuty = (uint32_t)((1 << _PwmTimers[timer].resolution) - 1);

	return std::make_unique<Esp32PwmControl>(pin, channel, timer, freq, maxDuty);
}



/**
 * @details This internal maintenance routine is triggered by the destruction 
 * of a PwmControl object. It performs the "Garbage Collection" of the hardware:
 * 
 * 1. Channel Recycling: Marks the LEDC channel as free in the occupancy bitmask.
 * 2. Timer Reference Counting: Decrements the client counter for the associated 
 *    timer. If the counter reaches zero, the timer's frequency is reset for 
 *    future lease.
 * 
 * This mechanism ensures that hardware resources are never leaked, even if 
 * a motor object is deleted unexpectedly.
 */

void Esp32PwmBroker::releaseResource(uint8_t channel, uint8_t timer) {
		// 1. Channel Release
	if (channel < LEDC_CHANNEL_MAX) {
		_PwmChannelsInUse[channel] = false;
	}

		// 2. Timer release
	if (timer < LEDC_TIMER_MAX) {
		if (_PwmTimers[timer].clients > 0) {
			_PwmTimers[timer].clients--;
		}

			// 3. Optional: Dynamic cleanup if no one is using this timer anymore
		if (_PwmTimers[timer].clients == 0) {
			_PwmTimers[timer].frequency = 0;
		}
	}
}





/**
 * @details The constructor initializes the internal state of the ESP32 PWM manager.
 * It resets the channel occupancy bitmask and clears the timer registry.
 * 
 * By being private, it strictly enforces the Singleton pattern, preventing
 * any accidental duplication of the hardware manager. This ensures that the 
 * 16 LEDC channels and 4 timers are always tracked by a single source of truth.
 */

Esp32PwmBroker::Esp32PwmBroker() {
		// Initialize all channels as free
	for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++) {
		_PwmChannelsInUse[i] = false;
	}
}



/**
 * @details This method manages the pool of 4 hardware timers.
 * It first looks for a timer with a matching frequency to share it.
 * If none is found, it picks a free timer and calculates the best 
 * possible bit resolution using a silent hardware test.
 */

int8_t Esp32PwmBroker::allocateTimer(uint32_t freq) {
		// --- 1. Search for an existing timer with the same frequency ---
	for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++) {
		if (_PwmTimers[i].clients > 0 && _PwmTimers[i].frequency == freq) {
			return (int8_t)i; // Found a match! Let's share.
		}
	}

		// --- 2. Search for a totally free timer slot ---
	for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++) {
		if (_PwmTimers[i].clients == 0) {
				// --- 3. Hardware Test (Automatic Resolution) ---
			ledc_timer_config_t config;
			config.speed_mode = LEDC_LOW_SPEED_MODE;
			config.timer_num  = (ledc_timer_t)i;
			config.freq_hz    = freq;
			config.clk_cfg    = LEDC_AUTO_CLK;

			uint8_t resBit = 14; // Start with high precision
			esp_log_set_vprintf(&Esp32PwmBroker::espSilentLog);

			while (resBit > 0) {
				_logHasOccured = 0;
				config.duty_resolution = (ledc_timer_bit_t)resBit;
				
				// Test if the ESP32 hardware accepts this resolution for this frequency
				if (ledc_timer_config(&config) == ESP_OK && _logHasOccured == 0) {
					break; // Found the best stable resolution!
				}
				resBit--;
			}
			esp_log_set_vprintf(&vprintf); // Restore standard serial logs

			// If a valid resolution was found
			if (resBit > 0) {
				_PwmTimers[i].frequency = freq;
				_PwmTimers[i].resolution = (ledc_timer_bit_t)resBit;
				return (int8_t)i;
			}
		}
	}

	return -1; // Error: No timers available or frequency incompatible
}