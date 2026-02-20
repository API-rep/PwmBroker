/**
 * @file Esp32PwmControl.h
 * @brief Esp32 specific implementation of the PwmControl interface.
 */

#pragma once

#include "PwmControl.h"
#include <driver/ledc.h>


/**
 * @class Esp32PwmControl
 * @brief Specialized handle for ESP32 LEDC hardware resources.
 * 
 * This class implements the PwmControl contract using the ESP-IDF LEDC driver.
 * It manages the lifecycle of a single PWM channel and its association 
 * with a hardware timer.
 */
class Esp32PwmControl : public PwmControl {

public:

		/// Constructor used exclusively by the Esp32PwmBroker.
	Esp32PwmControl(uint8_t pin, uint8_t channel, uint8_t timer, uint32_t freq, uint32_t maxDuty);
	
		/// Destructor ensuring the resource is released back to the Broker.
	virtual ~Esp32PwmControl();

		// --- setter methods ---
	bool setDuty(uint32_t value) override;    ///< Set duty cycle (0 to maxDuty).
	bool setFrequency(uint32_t hz) override;  ///< Change frequency (private due to virtual interface heritage).

		// --- getter methods ---
	uint32_t getDuty() const override;                                 ///< Get current duty cycle.
	uint32_t getMaxDuty() const override { return _maxDuty; }          ///< Get hardware resolution limit.
	uint32_t getFrequency() const override { return _frequency; }      ///< Get signal frequency in Hz.
	int8_t   getTimer() const override { return (int8_t)_timer; }      ///< Get associated hardware timer.
	uint8_t  getPwmPin() const override { return _pin; }               ///< Get physical GPIO pin.
	int8_t   getChannel() const override { return (int8_t)_channel; }  ///< Get assigned LEDC channel.


private:

	uint8_t        _pin;          ///< Physical GPIO pin number.
	ledc_channel_t _channel;      ///< ESP32 hardware channel (0-15).
	ledc_timer_t   _timer;        ///< Assigned hardware timer (0-3).
	uint32_t       _frequency;    ///< PWM signal frequency in Hz.
	uint32_t       _maxDuty;      ///< Maximum duty cycle value for current resolution.
};
