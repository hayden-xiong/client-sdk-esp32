#include "audio_activity_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "audio_activity";
static bool monitor_running = false;
static TaskHandle_t monitor_task_handle = NULL;

// Statistics tracking
static struct {
    uint32_t check_count;
    uint32_t heap_usage_samples;
    uint64_t total_heap_usage;
    uint32_t min_free_heap;
    uint32_t max_free_heap;
} activity_stats = {0};

static void audio_activity_monitor_task(void *pvParameters)
{
    const uint32_t check_interval_ms = 10000; // Check every 10 seconds
    const uint32_t detailed_report_interval = 6; // Detailed report every 60 seconds (6 * 10s)
    uint32_t detailed_counter = 0;
    
    ESP_LOGI(TAG, "Audio activity monitoring task started");
    
    while (monitor_running) {
        // Get current heap usage
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t free_internal_heap = esp_get_free_internal_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();
        
        // Update statistics
        activity_stats.check_count++;
        activity_stats.heap_usage_samples++;
        activity_stats.total_heap_usage += (heap_caps_get_total_size(MALLOC_CAP_DEFAULT) - free_heap);
        
        if (activity_stats.min_free_heap == 0 || free_heap < activity_stats.min_free_heap) {
            activity_stats.min_free_heap = free_heap;
        }
        if (free_heap > activity_stats.max_free_heap) {
            activity_stats.max_free_heap = free_heap;
        }
        
        // Basic activity logging
        ESP_LOGI(TAG, "🎵 AUDIO SYSTEM STATUS:");
        ESP_LOGI(TAG, "  Free heap: %lu bytes (internal: %lu bytes)", free_heap, free_internal_heap);
        ESP_LOGI(TAG, "  Min free heap ever: %lu bytes", min_free_heap);
        
        // Check if there's significant heap usage changes (indicating audio processing)
        static uint32_t last_free_heap = 0;
        if (last_free_heap > 0) {
            int32_t heap_change = (int32_t)free_heap - (int32_t)last_free_heap;
            if (abs(heap_change) > 1024) { // More than 1KB change
                ESP_LOGI(TAG, "  Heap change: %ld bytes (possible audio activity)", (long)heap_change);
            }
        }
        last_free_heap = free_heap;
        
        // Detailed report every minute
        detailed_counter++;
        if (detailed_counter >= detailed_report_interval) {
            detailed_counter = 0;
            
            ESP_LOGI(TAG, "📊 DETAILED AUDIO ACTIVITY REPORT:");
            ESP_LOGI(TAG, "  Total checks: %lu", activity_stats.check_count);
            ESP_LOGI(TAG, "  Heap range: %lu - %lu bytes", 
                     activity_stats.min_free_heap, activity_stats.max_free_heap);
            
            if (activity_stats.heap_usage_samples > 0) {
                uint32_t avg_heap_usage = activity_stats.total_heap_usage / activity_stats.heap_usage_samples;
                ESP_LOGI(TAG, "  Average heap usage: %lu bytes", avg_heap_usage);
            }
            
            // Check task status
            UBaseType_t task_count = uxTaskGetNumberOfTasks();
            ESP_LOGI(TAG, "  Active tasks: %lu", (uint32_t)task_count);
            
            // System uptime
            uint64_t uptime_us = esp_timer_get_time();
            uint32_t uptime_seconds = uptime_us / 1000000;
            ESP_LOGI(TAG, "  System uptime: %lu seconds", uptime_seconds);
            
            ESP_LOGI(TAG, "🎤 MICROPHONE STATUS: Monitor for consistent heap usage patterns");
            ESP_LOGI(TAG, "🔊 SPEAKER STATUS: Monitor for heap allocation spikes during playback");
            ESP_LOGI(TAG, "----------------------------------------");
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
    }
    
    ESP_LOGI(TAG, "Audio activity monitoring task stopped");
    monitor_task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_activity_monitor_start(void)
{
    if (!monitor_running) {
        monitor_running = true;
        
        // Reset statistics
        memset(&activity_stats, 0, sizeof(activity_stats));
        
        BaseType_t result = xTaskCreate(
            audio_activity_monitor_task,
            "audio_activity",
            4096,  // Stack size
            NULL,  // Parameters
            3,     // Priority (lower than audio tasks)
            &monitor_task_handle
        );
        
        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create audio activity monitoring task");
            monitor_running = false;
        } else {
            ESP_LOGI(TAG, "Audio activity monitoring started");
        }
    }
}

void audio_activity_monitor_stop(void)
{
    if (monitor_running) {
        monitor_running = false;
        ESP_LOGI(TAG, "Stopping audio activity monitoring...");
    }
}

void audio_activity_log_status(void)
{
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t free_internal_heap = esp_get_free_internal_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    
    ESP_LOGI(TAG, "=== MANUAL AUDIO SYSTEM STATUS ===");
    ESP_LOGI(TAG, "Free heap: %lu bytes", free_heap);
    ESP_LOGI(TAG, "Free internal heap: %lu bytes", free_internal_heap);
    ESP_LOGI(TAG, "Minimum free heap ever: %lu bytes", min_free_heap);
    ESP_LOGI(TAG, "Active tasks: %lu", (uint32_t)task_count);
    ESP_LOGI(TAG, "Check count: %lu", activity_stats.check_count);
    ESP_LOGI(TAG, "==================================");
}