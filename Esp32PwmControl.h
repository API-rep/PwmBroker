/**
 * @file Esp32PwmControl.h
 * @brief Esp32 specific implementation of the PwmControl interface.
 */

#pragma once

#include "PwmControl.h"
#include <driver/ledc.h>

class Esp32PwmControl : public PwmControl {
public:
		// Constructor used only by the Esp32PwmBroker
	Esp32PwmControl(uint8_t pin, uint8_t channel, uint8_t timer, uint32_t freq, uint32_t maxDuty);
	
		// Destructor: This is where the "release call" to the Broker happens
	virtual ~Esp32PwmControl();

		// --- Real Implementation of PwmControl.h virtual Methods ---
	bool setDuty(uint32_t value) override;
	bool setFrequency(uint32_t hz) override;

	uint32_t getDuty() const override;
	uint32_t getMaxDuty() const override { return _maxDuty; }
	uint32_t getFrequency() const override { return _frequency; }
	uint8_t  getPwmPin() const override { return _pin; }
	int8_t   getChannel() const override { return (int8_t)_channel; }

private:
	uint8_t        _pin;
	ledc_channel_t _channel;
	ledc_timer_t   _timer;
	uint32_t       _frequency;
	uint32_t       _maxDuty;
};