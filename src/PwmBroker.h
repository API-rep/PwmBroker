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

enum class PwmModeRequest : uint8_t {
	Auto = 0,
	LowSpeed,
	HighSpeed
};

class PwmBroker {

public:

    /// Access the unique instance of the Broker (Singleton).
	static PwmBroker& getInstance();

    /// Prototype for requesting a PWM resource (Lease) for a specific pin at given frequency and mode.
	/// @param timerHint   Preferred timer index (-1 = no preference).
	/// @param channelHint Preferred channel index (-1 = no preference).
	virtual std::unique_ptr<PwmControl> requestResource(uint8_t pin, uint32_t freq,
	                                                     PwmModeRequest modeRequest = PwmModeRequest::Auto,
	                                                     int8_t timerHint   = -1,
	                                                     int8_t channelHint = -1) = 0;


protected:

		// Constructors protected to enforce Singleton pattern via specialized classes.
	PwmBroker() = default;
	virtual ~PwmBroker() = default;

		// Prevent cloning of the instance to guarantee the Singleton's uniqueness.
	PwmBroker(const PwmBroker&) = delete;

    // Prevent overwriting the instance through assignment operations.
	PwmBroker& operator=(const PwmBroker&) = delete;
};
