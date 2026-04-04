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
#include <esp_log.h>
#include <soc/soc_caps.h>
#include <cstdio>

int Esp32PwmBroker::_logHasOccured = -1;



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

std::unique_ptr<PwmControl> Esp32PwmBroker::requestResource(uint8_t pin, uint32_t freq, PwmModeRequest modeRequest, int8_t timerHint, int8_t channelHint) {
	for (uint8_t attempt = 0; attempt < 2; attempt++) {
		ledc_mode_t mode = LEDC_LOW_SPEED_MODE;

		switch (modeRequest) {
			case PwmModeRequest::LowSpeed:
				if (attempt > 0) {
					return nullptr;
				}
				mode = LEDC_LOW_SPEED_MODE;
				break;

			case PwmModeRequest::HighSpeed:
				if (attempt > 0) {
					return nullptr;
				}
				mode = LEDC_HIGH_SPEED_MODE;
				break;

			case PwmModeRequest::Auto:
			default:
				mode = (attempt == 0) ? LEDC_HIGH_SPEED_MODE : LEDC_LOW_SPEED_MODE;
				break;
		}

		if (!isModeSupported(mode)) {
			continue;
		}

		uint8_t modeIdx = modeIndex(mode);

		// --- 1. Find channel in selected mode (hint preferred, then first free) ---
		int8_t channel = -1;
		if (channelHint >= 0 && channelHint < (int8_t)LEDC_CHANNEL_MAX
		    && !_PwmChannelsInUse[modeIdx][(uint8_t)channelHint]) {
			channel = channelHint;
		} else {
			for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++) {
				if (!_PwmChannelsInUse[modeIdx][i]) {
					channel = i;
					break;
				}
			}
		}
		if (channel == -1) {
			continue;
		}

		  // --- 2. Find or allocate a timer in selected mode (hint preferred) ---
		int8_t timer = allocateTimer(mode, freq, timerHint);
		if (timer == -1) {
			continue;
		}

		_PwmTimers[modeIdx][timer].clients++;

		  // --- 3. Hardware configuration (LEDC Channel only) ---
		ledc_channel_config_t config = {
			.gpio_num   = pin,
			.speed_mode = mode,
			.channel    = (ledc_channel_t)channel,
			.intr_type  = LEDC_INTR_DISABLE,
			.timer_sel  = (ledc_timer_t)timer,
			.duty       = 0,
			.hpoint     = 0
		};

		if (ledc_channel_config(&config) != ESP_OK) {
			releaseResource((uint8_t)channel, (uint8_t)timer, mode);
			continue;
		}

		  // --- 4. Update channel and timer clients lease data ---
		_PwmChannelsInUse[modeIdx][channel] = true;

		uint32_t maxDuty = (uint32_t)((1 << _PwmTimers[modeIdx][timer].resolution) - 1);
		return std::make_unique<Esp32PwmControl>(pin, channel, timer, mode, freq, maxDuty);
	}

	return nullptr;
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

void Esp32PwmBroker::releaseResource(uint8_t channel, uint8_t timer, ledc_mode_t mode) {
	if (!isModeSupported(mode)) {
		return;
	}

	uint8_t modeIdx = modeIndex(mode);

		// 1. Channel Release
	if (channel < LEDC_CHANNEL_MAX) {
		_PwmChannelsInUse[modeIdx][channel] = false;
	}

		// 2. Timer release
	if (timer < LEDC_TIMER_MAX) {
		if (_PwmTimers[modeIdx][timer].clients > 0) {
			_PwmTimers[modeIdx][timer].clients--;
		}

			// 3. Optional: Dynamic cleanup if no one is using this timer anymore
		if (_PwmTimers[modeIdx][timer].clients == 0) {
			_PwmTimers[modeIdx][timer].frequency = 0;
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
		// Initialize all channels as free for each mode
	for (uint8_t modeIdx = 0; modeIdx < 2; modeIdx++) {
		for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++) {
			_PwmChannelsInUse[modeIdx][i] = false;
		}
	}
}



/**
 * @details This method manages the pool of 4 hardware timers.
 * It first looks for a timer with a matching frequency to share it.
 * If none is found, it picks a free timer and calculates the best 
 * possible bit resolution using a silent hardware test.
 */

int8_t Esp32PwmBroker::allocateTimer(ledc_mode_t mode, uint32_t freq, int8_t hint) {
	if (!isModeSupported(mode)) {
		return -1;
	}

	uint8_t modeIdx = modeIndex(mode);

		// --- 1. Search for an existing timer with the same frequency (shared, no hint needed) ---
	for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++) {
		if (_PwmTimers[modeIdx][i].clients > 0 && _PwmTimers[modeIdx][i].frequency == freq) {
			return (int8_t)i;
		}
	}

		// --- 2. Discover and configure a free timer; try hint first, then scan ---
	auto tryConfigureTimer = [&](uint8_t i) -> int8_t {
		if (_PwmTimers[modeIdx][i].clients > 0) return -1;  // already in use

		ledc_timer_config_t config;
		config.speed_mode = mode;
		config.timer_num  = (ledc_timer_t)i;
		config.freq_hz    = freq;
		config.clk_cfg    = LEDC_AUTO_CLK;

		uint8_t resBit = 14;
		esp_log_set_vprintf(&Esp32PwmBroker::espSilentLog);

		while (resBit > 0) {
			_logHasOccured = 0;
			config.duty_resolution = (ledc_timer_bit_t)resBit;

			if (ledc_timer_config(&config) == ESP_OK && _logHasOccured == 0) {
				break;
			}
			resBit--;
		}
		esp_log_set_vprintf(&vprintf);

		if (resBit > 0) {
			_PwmTimers[modeIdx][i].frequency  = freq;
			_PwmTimers[modeIdx][i].resolution = (ledc_timer_bit_t)resBit;
			return (int8_t)i;
		}
		return -1;
	};

	if (hint >= 0 && hint < (int8_t)LEDC_TIMER_MAX) {
		int8_t r = tryConfigureTimer((uint8_t)hint);
		if (r >= 0) return r;
	}

	for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++) {
		if (i == (uint8_t)hint) continue;  // already tried
		int8_t r = tryConfigureTimer(i);
		if (r >= 0) return r;
	}

	return -1;
}

int Esp32PwmBroker::espSilentLog(const char* string, va_list args) {
	_logHasOccured = vsnprintf(nullptr, 0, string, args);
	return 0;
}

uint8_t Esp32PwmBroker::modeIndex(ledc_mode_t mode) {
	return (mode == LEDC_HIGH_SPEED_MODE) ? 1 : 0;
}

bool Esp32PwmBroker::isModeSupported(ledc_mode_t mode) {
	if (mode == LEDC_LOW_SPEED_MODE) {
		return true;
	}

#if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
	return true;
#else
	(void)mode;
	return false;
#endif
}
