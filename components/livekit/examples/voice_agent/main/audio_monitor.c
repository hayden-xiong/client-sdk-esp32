#include "audio_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "audio_monitor";

// Monitor state
static bool monitor_initialized = false;
static bool monitor_enabled = false;
static audio_monitor_config_t monitor_config;

// Statistics
static audio_stats_t mic_stats = {0};
static audio_stats_t speaker_stats = {0};

// Timing for periodic logging
static uint64_t last_mic_log_time = 0;
static uint64_t last_speaker_log_time = 0;

// Helper function to calculate RMS volume from audio data
static uint32_t calculate_rms_volume(const uint8_t *data, size_t len, uint8_t bits_per_sample)
{
    if (!data || len == 0) {
        return 0;
    }

    uint64_t sum_squares = 0;
    size_t sample_count = 0;

    if (bits_per_sample == 16) {
        const int16_t *samples = (const int16_t *)data;
        sample_count = len / 2;
        
        for (size_t i = 0; i < sample_count; i++) {
            int32_t sample = samples[i];
            sum_squares += sample * sample;
        }
    } else if (bits_per_sample == 8) {
        sample_count = len;
        
        for (size_t i = 0; i < sample_count; i++) {
            int16_t sample = (int16_t)data[i] - 128; // Convert unsigned to signed
            sum_squares += sample * sample;
        }
    } else {
        ESP_LOGW(TAG, "Unsupported bits per sample: %d", bits_per_sample);
        return 0;
    }

    if (sample_count == 0) {
        return 0;
    }

    // Calculate RMS
    double mean_square = (double)sum_squares / sample_count;
    uint32_t rms = (uint32_t)sqrt(mean_square);
    
    return rms;
}

// Helper function to update statistics
static void update_stats(audio_stats_t *stats, const uint8_t *data, size_t len,
                        uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample)
{
    if (!stats || !data) {
        return;
    }

    uint32_t volume = calculate_rms_volume(data, len, bits_per_sample);
    
    stats->total_frames++;
    stats->total_bytes += len;
    
    if (volume < monitor_config.volume_threshold) {
        stats->silent_frames++;
    } else {
        stats->active_frames++;
    }
    
    if (volume > stats->peak_volume) {
        stats->peak_volume = volume;
    }
    
    // Update average volume (simple moving average)
    stats->avg_volume = (stats->avg_volume * (stats->total_frames - 1) + volume) / stats->total_frames;
}

int audio_monitor_init(const audio_monitor_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return -1;
    }

    memcpy(&monitor_config, config, sizeof(audio_monitor_config_t));
    
    // Reset statistics
    memset(&mic_stats, 0, sizeof(audio_stats_t));
    memset(&speaker_stats, 0, sizeof(audio_stats_t));
    
    last_mic_log_time = 0;
    last_speaker_log_time = 0;
    
    monitor_initialized = true;
    monitor_enabled = true;
    
    ESP_LOGI(TAG, "Audio monitor initialized");
    ESP_LOGI(TAG, "  Mic logging: %s", config->enable_mic_logging ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Speaker logging: %s", config->enable_speaker_logging ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Log interval: %lu ms", config->log_interval_ms);
    ESP_LOGI(TAG, "  Volume threshold: %lu", config->volume_threshold);
    
    return 0;
}

void audio_monitor_mic_input(const uint8_t *data, size_t len,
                            uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample)
{
    if (!monitor_initialized || !monitor_enabled || !monitor_config.enable_mic_logging) {
        return;
    }

    if (!data || len == 0) {
        return;
    }

    // Update statistics
    update_stats(&mic_stats, data, len, sample_rate, channels, bits_per_sample);
    
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    // Log periodically
    if (current_time - last_mic_log_time >= monitor_config.log_interval_ms) {
        uint32_t volume = calculate_rms_volume(data, len, bits_per_sample);
        
        if (volume >= monitor_config.volume_threshold) {
            ESP_LOGI(TAG, "🎤 MIC INPUT: %zu bytes, %luHz, %dch, %dbit, vol=%lu, frames=%lu",
                     len, sample_rate, channels, bits_per_sample, volume, mic_stats.total_frames);
        } else {
            ESP_LOGD(TAG, "🎤 MIC INPUT (quiet): %zu bytes, vol=%lu (threshold=%lu)",
                     len, volume, monitor_config.volume_threshold);
        }
        
        last_mic_log_time = current_time;
    }
}

void audio_monitor_speaker_output(const uint8_t *data, size_t len,
                                 uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample)
{
    if (!monitor_initialized || !monitor_enabled || !monitor_config.enable_speaker_logging) {
        return;
    }

    if (!data || len == 0) {
        return;
    }

    // Update statistics
    update_stats(&speaker_stats, data, len, sample_rate, channels, bits_per_sample);
    
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    // Log periodically
    if (current_time - last_speaker_log_time >= monitor_config.log_interval_ms) {
        uint32_t volume = calculate_rms_volume(data, len, bits_per_sample);
        
        if (volume >= monitor_config.volume_threshold) {
            ESP_LOGI(TAG, "🔊 SPEAKER OUTPUT: %zu bytes, %luHz, %dch, %dbit, vol=%lu, frames=%lu",
                     len, sample_rate, channels, bits_per_sample, volume, speaker_stats.total_frames);
        } else {
            ESP_LOGD(TAG, "🔊 SPEAKER OUTPUT (quiet): %zu bytes, vol=%lu (threshold=%lu)",
                     len, volume, monitor_config.volume_threshold);
        }
        
        last_speaker_log_time = current_time;
    }
}

void audio_monitor_get_mic_stats(audio_stats_t *stats)
{
    if (stats && monitor_initialized) {
        memcpy(stats, &mic_stats, sizeof(audio_stats_t));
    }
}

void audio_monitor_get_speaker_stats(audio_stats_t *stats)
{
    if (stats && monitor_initialized) {
        memcpy(stats, &speaker_stats, sizeof(audio_stats_t));
    }
}

void audio_monitor_reset_stats(void)
{
    if (monitor_initialized) {
        memset(&mic_stats, 0, sizeof(audio_stats_t));
        memset(&speaker_stats, 0, sizeof(audio_stats_t));
        ESP_LOGI(TAG, "Audio statistics reset");
    }
}

void audio_monitor_set_enabled(bool enable)
{
    if (monitor_initialized) {
        monitor_enabled = enable;
        ESP_LOGI(TAG, "Audio monitoring %s", enable ? "enabled" : "disabled");
    }
}