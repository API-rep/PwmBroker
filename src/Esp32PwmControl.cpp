/**
 * @file Esp32PwmControl.cpp
 * @brief Implementation of the Esp32 PWM resource handle.
 * @details This class is the physical handle to an ESP32 LEDC channel. 
 * It acts as a specialized proxy between the generic PwmControl interface 
 * and the ESP-IDF driver.
 * 
 * Life Cycle Management (RAII):
 * The most critical part of this implementation is the destructor. When 
 * a motor (the owner) is deleted, this object is automatically destroyed, 
 * triggering a hardware stop and a "release" call to the PwmBroker. 
 * This ensures that hardware channels and timers are never leaked, 
 * maintaining the integrity of the 16-channel pool.
 */

#include "Esp32PwmControl.h"
#include "Esp32PwmBroker.h"


/**
 * @details The constructor is called by the Broker after physical 
 * resource allocation. It simply stores the hardware IDs (pin, channel, 
 * timer) and the pre-calculated max duty value. It does not perform 
 * any hardware configuration, as the Broker handles that during the 'Lease'.
 */

Esp32PwmControl::Esp32PwmControl(uint8_t pin, uint8_t channel, uint8_t timer, uint32_t freq, uint32_t maxDuty) 
	: _pin(pin), _channel((ledc_channel_t)channel), _timer((ledc_timer_t)timer), _frequency(freq), _maxDuty(maxDuty) {
}



/**
 * @details The destructor ensures hardware safety and resource recycling.
 * 
 * 1. Hardware Stop: It forces the LEDC channel to stop immediately, 
 *    preventing the PWM signal from persisting after the object is deleted.
 * 2. Resource Release: It notifies the Broker to free up the channel and 
 *    decrement the timer's client counter (RAII pattern).
 */
Esp32PwmControl::~Esp32PwmControl() {
		// --- 1. Stop PWM output immediately for safety ---
	ledc_stop(LEDC_LOW_SPEED_MODE, _channel, 0);

		// --- 2. Notify the Broker to release hardware resources ---
	Esp32PwmBroker::getInstance().releaseResource((uint8_t)_channel, (uint8_t)_timer);
}




/**
 * @details Direct hardware update of the duty cycle.
 * It uses 'ledc_set_duty_and_update' for immediate effect at the next
 * PWM cycle, bypassing the fader for maximum reactivity.
 * 
 * @return True if the ESP32 hardware register was updated successfully.
 */
bool Esp32PwmControl::setDuty(uint32_t value) {
	return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, _channel, value, 0) == ESP_OK;
}



/**
 * @details Updates the hardware timer frequency. 
 * 
 * @note Although public in this implementation, this method is 'protected' in 
 * the PwmControl interface. This prevents end-users (like motors) from 
 * changing the frequency at runtime, which would disrupt all shared resources 
 * on the same timer. Only the Broker or specialized system classes should 
 * access this.
 * 
 * @return True if the frequency was successfully applied to the timer.
 */

bool Esp32PwmControl::setFrequency(uint32_t hz) {
	if (ledc_set_freq(LEDC_LOW_SPEED_MODE, _timer, hz) == ESP_OK) {
		_frequency = hz;
		return true;
	}
	return false;
}



/**
 * @details Returns the actual duty cycle value from the hardware register.
 * 
 * 1. Synchronicity: This reflects the current state of the LEDC channel 
 *    after any pending updates have been applied.
 * 2. Raw Value: The returned value is relative to the hardware resolution 
 *    (MaxDuty) calculated during the Broker's allocation.
 * 
 * @return The current raw duty cycle value (0 to MaxDuty).
 */

uint32_t Esp32PwmControl::getDuty() const {
		// Fetch directly from the ESP32 hardware channel register
	return ledc_get_duty(LEDC_LOW_SPEED_MODE, _channel);
}
