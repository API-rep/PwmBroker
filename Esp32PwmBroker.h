/**
 * @file Esp32PwmBroker.h
 * @brief ESP32 specific implementation of the PWM Broker.
 * 
 * @details This class realizes the PwmBroker contract for the ESP32 architecture.
 * It manages the 16 LEDC channels and 4 hardware timers, implementing 
 * resource sharing (mutualization) and automatic resolution discovery.
 */
#pragma once

#include "PwmBroker.h"
#include <cstdarg>
#include <driver/ledc.h>


 /** 
* @brief ESP32 specialized executor for the generic PWM Broker interface.
* Manages LEDC hardware resources (16 channels, 4 timers).
*/
class Esp32PwmBroker : public PwmBroker {

public:

    /// Returns the unique instance of the ESP32-specific Broker.
  static Esp32PwmBroker& getInstance();

    /// Realize the PWM lease by allocating physical ESP32 resources.
  std::unique_ptr<PwmControl> requestResource(uint8_t pin, uint32_t freq) override;

    /// Release hardware resources and update internal tracking data.
	void releaseResource(uint8_t channel, uint8_t timer);

  

private:

		/// Private constructor to ensure instantiation only via getInstance().
	Esp32PwmBroker();

		/// Internal structure to track hardware timer allocation and sharing.
	struct PwmTimer {
		uint32_t frequency = 0;       ///< Configured frequency in Hz.
		uint8_t  clients   = 0;       ///< Number of resources sharing this timer.
		ledc_timer_bit_t resolution;  ///< Max stable resolution found for this frequency.
	};

  	/// Registry of hardware timers and their current usage.
	PwmTimer  _PwmTimers[LEDC_TIMER_MAX];

    /// Array to track channels usage.
  bool  _PwmChannelsInUse[LEDC_CHANNEL_MAX];

    /// Find a suitable exisiting timer or allocate a new one for the requested frequency.
  int8_t allocateTimer(uint32_t freq);

	  /// Silent logger used to probe timer resolution without polluting serial logs.
	static int espSilentLog(const char* string, va_list args);

	  /// Internal flag set when ESP-IDF emits an error during timer probing.
	static int _logHasOccured;
};