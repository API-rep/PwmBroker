/**
 * @file Esp32PwmControl.cpp
 * @brief Implementation of the Esp32 PWM resource handle.
 */

#include "Esp32PwmControl.h"
#include "Esp32PwmBroker.h"

Esp32PwmControl::Esp32PwmControl(uint8_t pin, uint8_t channel, uint8_t timer, uint32_t freq, uint32_t maxDuty) 
	: _pin(pin), _channel((ledc_channel_t)channel), _timer((ledc_timer_t)timer), _frequency(freq), _maxDuty(maxDuty) {
		// Hardware is already configured by the Broker before creating this object
}


/**
 * @brief Destroy the Esp32 Pwm Control object
 * @details Free the hardware resource by calling the Broker's release function.
 * 
 */
Esp32PwmControl::~Esp32PwmControl() {
		// --- 1. Stop PWM output before releasing the hardware ---
	ledc_stop(LEDC_LOW_SPEED_MODE, _channel, 0);

		// --- 2. Call the Broker to manage freeup resources
	Esp32PwmBroker::getInstance().releaseResource((uint8_t)_channel, (uint8_t)_timer);
}



bool Esp32PwmControl::setDuty(uint32_t value) {
		// Immediate hardware update
	return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, _channel, value, 0) == ESP_OK;
}

bool Esp32PwmControl::setFrequency(uint32_t hz) {
		// --- Tushy Point ---
		// Changing frequency on a shared timer affects ALL motors on that timer.
		// For now, we update it, but the Broker should manage this conflict later.
	if (ledc_set_freq(LEDC_LOW_SPEED_MODE, _timer, hz) == ESP_OK) {
		_frequency = hz;
		return true;
	}
	return false;
}

uint32_t Esp32PwmControl::getDuty() const {
	return ledc_get_duty(LEDC_LOW_SPEED_MODE, _channel);
}