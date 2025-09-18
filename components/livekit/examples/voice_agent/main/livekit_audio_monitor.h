#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LiveKit audio monitoring
 * 
 * This will hook into the LiveKit room callbacks to monitor
 * actual audio activity when the room is connected.
 */
void livekit_audio_monitor_init(void);

/**
 * @brief Check LiveKit room audio status
 */
void livekit_audio_monitor_check_status(void);

/**
 * @brief Force start audio capture test
 * 
 * This will attempt to manually trigger audio capture
 * to test if the hardware is working.
 */
void livekit_audio_monitor_test_capture(void);

#ifdef __cplusplus
}
#endif