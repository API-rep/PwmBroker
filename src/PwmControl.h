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
	virtual bool setDuty(uint32_t value) = 0;   ///< Set duty cycle (0 to maxDuty).

		// --- Getters Methods ---
	virtual uint32_t getDuty() const = 0;       ///< Get current duty cycle value. 
	virtual uint32_t getMaxDuty() const = 0;    ///< Get hardware resolution limit.
	virtual uint32_t getFrequency() const = 0;  ///< Get signal frequency in Hz.
	virtual int8_t   getTimer() const = 0;      ///< Get assigned hardware timer index.
	virtual uint8_t  getPwmPin() const = 0;     ///< Get physical GPIO pin number.
	virtual int8_t   getChannel() const = 0;    ///< Get assigned hardware channel.

  	/// Virtual destructor to ensure proper hardware cleanup.
	virtual ~PwmControl() = default;


protected:

		/// Change PWM frequency (Protected: restricted to Broker/System use).
	virtual bool setFrequency(uint32_t hz) = 0;
};
