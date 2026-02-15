/**
 * @file PwmBroker.h
 * @brief Singleton interface for PWM resource management (Broker).
 * 
 * @details This class acts as a Matchmaker between high-level controllers 
 * and hardware. It manages resource leasing and ensures optimal allocation 
 * of timers and channels across different architectures.
 */


#pragma once

#include "PwmControl.h"
#include <memory>

class PwmBroker {

public:
    // Ensure access the UNIQUE instance of the Broker.
    // Will be relayed to the architecture-specific implementation (Esp32PwmBroker) through polymorphism.
    /// Access the unique instance of the Broker (Singleton).
	static PwmBroker& getInstance();

    /// Prototype for requesting a PWM resource (Lease) for a specific pin and frequency.
	virtual std::unique_ptr<PwmControl> requestResource(uint8_t pin, uint32_t freq) = 0;


protected:
		// Protected constructor and destructor. Only accessible via specialized child classes.
    // "default" ensures to build a standard version of the constructor and destructor
    // It can be overridden by child classes if needed.
	PwmBroker() = default;
	virtual ~PwmBroker() = default;

		// Prevent cloning of the instance to guarantee the Singleton's uniqueness.
	PwmBroker(const PwmBroker&) = delete;
    // Prevent overwriting the instance through assignment operations.
	PwmBroker& operator=(const PwmBroker&) = delete;
};