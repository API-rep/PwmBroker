/**
 * @file PwmControl.h
 * @brief PWM control generic interface for hardware abstraction.
 * 
 * @details This interface defines the universal controls for PWM resources.
 * It allows high-level controllers (like motors) to manipulate hardware 
 * without knowing the specific processor architecture (Esp32, STM32, etc.).
 * Implementation is delegated to specialized classes through polymorphism.
 */

 #pragma once
#include <stdint.h>

class PwmControl {
public:

		// --- Control Methods ---
	virtual bool setDuty(uint32_t value) = 0;

		// --- Telemetry Methods ---
	virtual uint32_t getDuty() const = 0;
	virtual uint32_t getMaxDuty() const = 0;
	virtual uint32_t getFrequency() const = 0;
	virtual int8_t   getTimer() const = 0;
	virtual uint8_t  getPwmPin() const = 0;
	virtual int8_t   getChannel() const = 0;

protected:

		// --- Control Methods ---
	virtual bool setFrequency(uint32_t hz) = 0;

  	// Virtual destructor for proper hardware cleanup
	virtual ~PwmControl() = default;
};