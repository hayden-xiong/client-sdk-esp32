#include "audio_monitor.h"
#include "livekit_audio_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_stats";
static bool stats_task_running = false;
static TaskHandle_t stats_task_handle = NULL;

// Task to periodically report audio statistics
static void audio_stats_task(void *pvParameters)
{
    const uint32_t report_interval_ms = 30000; // Report every 30 seconds
    
    ESP_LOGI(TAG, "Audio statistics reporting task started");
    
    while (stats_task_running) {
        vTaskDelay(pdMS_TO_TICKS(report_interval_ms));
        
        if (!stats_task_running) {
            break;
        }
        
        audio_stats_t mic_stats, speaker_stats;
        audio_monitor_get_mic_stats(&mic_stats);
        audio_monitor_get_speaker_stats(&speaker_stats);
        
        // Report microphone statistics
        if (mic_stats.total_frames > 0) {
            float active_percentage = (float)mic_stats.active_frames * 100.0f / mic_stats.total_frames;
            ESP_LOGI(TAG, "📊 MIC STATS:");
            ESP_LOGI(TAG, "  Total frames: %lu, Active: %lu (%.1f%%), Silent: %lu", 
                     mic_stats.total_frames, mic_stats.active_frames, active_percentage, mic_stats.silent_frames);
            ESP_LOGI(TAG, "  Peak volume: %lu, Avg volume: %lu, Total bytes: %llu",
                     mic_stats.peak_volume, mic_stats.avg_volume, mic_stats.total_bytes);
        } else {
            ESP_LOGI(TAG, "📊 MIC STATS: No audio input detected");
        }
        
        // Report speaker statistics
        if (speaker_stats.total_frames > 0) {
            float active_percentage = (float)speaker_stats.active_frames * 100.0f / speaker_stats.total_frames;
            ESP_LOGI(TAG, "📊 SPEAKER STATS:");
            ESP_LOGI(TAG, "  Total frames: %lu, Active: %lu (%.1f%%), Silent: %lu", 
                     speaker_stats.total_frames, speaker_stats.active_frames, active_percentage, speaker_stats.silent_frames);
            ESP_LOGI(TAG, "  Peak volume: %lu, Avg volume: %lu, Total bytes: %llu",
                     speaker_stats.peak_volume, speaker_stats.avg_volume, speaker_stats.total_bytes);
        } else {
            ESP_LOGI(TAG, "📊 SPEAKER STATS: No audio output detected");
        }
        
        // Also check LiveKit-specific status
        livekit_audio_monitor_check_status();
        
        ESP_LOGI(TAG, "----------------------------------------");
    }
    
    ESP_LOGI(TAG, "Audio statistics reporting task stopped");
    stats_task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_stats_start_reporting(void)
{
    if (!stats_task_running) {
        stats_task_running = true;
        
        BaseType_t result = xTaskCreate(
            audio_stats_task,
            "audio_stats",
            4096,  // Stack size
            NULL,  // Parameters
            5,     // Priority
            &stats_task_handle
        );
        
        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create audio statistics task");
            stats_task_running = false;
        } else {
            ESP_LOGI(TAG, "Audio statistics reporting started");
        }
    }
}

void audio_stats_stop_reporting(void)
{
    if (stats_task_running) {
        stats_task_running = false;
        ESP_LOGI(TAG, "Stopping audio statistics reporting...");
    }
}