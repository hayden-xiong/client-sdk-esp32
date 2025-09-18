#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start audio activity monitoring task
 * 
 * This task will periodically check if there's audio activity
 * by monitoring system resources and audio pipeline status.
 */
void audio_activity_monitor_start(void);

/**
 * @brief Stop audio activity monitoring task
 */
void audio_activity_monitor_stop(void);

/**
 * @brief Manually log audio pipeline status
 */
void audio_activity_log_status(void);

#ifdef __cplusplus
}
#endif