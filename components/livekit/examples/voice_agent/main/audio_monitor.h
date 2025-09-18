#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

// Audio monitoring configuration
typedef struct {
    bool enable_mic_logging;      // Enable microphone input logging
    bool enable_speaker_logging;  // Enable speaker output logging
    uint32_t log_interval_ms;     // Logging interval in milliseconds
    uint32_t volume_threshold;    // Minimum volume threshold for logging
} audio_monitor_config_t;

// Audio statistics
typedef struct {
    uint32_t total_frames;        // Total audio frames processed
    uint32_t silent_frames;       // Frames below threshold
    uint32_t active_frames;       // Frames above threshold
    uint32_t peak_volume;         // Peak volume detected
    uint32_t avg_volume;          // Average volume
    uint64_t total_bytes;         // Total bytes processed
} audio_stats_t;

/**
 * @brief Initialize audio monitoring
 * 
 * @param config Monitoring configuration
 * @return 0 on success, -1 on failure
 */
int audio_monitor_init(const audio_monitor_config_t *config);

/**
 * @brief Monitor microphone input data
 * 
 * @param data Audio data buffer
 * @param len Data length in bytes
 * @param sample_rate Sample rate in Hz
 * @param channels Number of channels
 * @param bits_per_sample Bits per sample
 */
void audio_monitor_mic_input(const uint8_t *data, size_t len, 
                            uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample);

/**
 * @brief Monitor speaker output data
 * 
 * @param data Audio data buffer
 * @param len Data length in bytes
 * @param sample_rate Sample rate in Hz
 * @param channels Number of channels
 * @param bits_per_sample Bits per sample
 */
void audio_monitor_speaker_output(const uint8_t *data, size_t len,
                                 uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample);

/**
 * @brief Get microphone statistics
 * 
 * @param stats Pointer to store statistics
 */
void audio_monitor_get_mic_stats(audio_stats_t *stats);

/**
 * @brief Get speaker statistics
 * 
 * @param stats Pointer to store statistics
 */
void audio_monitor_get_speaker_stats(audio_stats_t *stats);

/**
 * @brief Reset audio statistics
 */
void audio_monitor_reset_stats(void);

/**
 * @brief Enable/disable monitoring
 * 
 * @param enable true to enable, false to disable
 */
void audio_monitor_set_enabled(bool enable);

#ifdef __cplusplus
}
#endif