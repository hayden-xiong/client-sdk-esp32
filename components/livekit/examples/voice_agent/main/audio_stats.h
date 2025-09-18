#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start periodic audio statistics reporting
 * 
 * This function starts a background task that periodically reports
 * audio statistics including microphone input and speaker output metrics.
 */
void audio_stats_start_reporting(void);

/**
 * @brief Stop periodic audio statistics reporting
 * 
 * This function stops the background statistics reporting task.
 */
void audio_stats_stop_reporting(void);

#ifdef __cplusplus
}
#endif