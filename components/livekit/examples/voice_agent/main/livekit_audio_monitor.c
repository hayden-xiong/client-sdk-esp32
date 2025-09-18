#include "livekit_audio_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "media.h"

static const char *TAG = "livekit_audio";

// Simple statistics
static struct {
    uint32_t status_checks;
    bool room_connected;
    uint64_t last_activity_time;
} monitor_stats = {0};

void livekit_audio_monitor_init(void)
{
    ESP_LOGI(TAG, "LiveKit audio monitor initialized");
    monitor_stats.status_checks = 0;
    monitor_stats.room_connected = false;
    monitor_stats.last_activity_time = 0;
}

void livekit_audio_monitor_check_status(void)
{
    monitor_stats.status_checks++;
    
    ESP_LOGI(TAG, "🎵 LIVEKIT AUDIO STATUS CHECK #%lu:", monitor_stats.status_checks);
    
    // Get current system time
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    // Check if we have audio handles
    esp_capture_handle_t capturer = media_get_capturer();
    av_render_handle_t renderer = media_get_renderer();
    
    ESP_LOGI(TAG, "  📊 System Status:");
    ESP_LOGI(TAG, "    Capturer handle: %s", capturer ? "✅ Available" : "❌ NULL");
    ESP_LOGI(TAG, "    Renderer handle: %s", renderer ? "✅ Available" : "❌ NULL");
    ESP_LOGI(TAG, "    System uptime: %llu seconds", current_time / 1000);
    
    ESP_LOGI(TAG, "  🔍 Audio Analysis:");
    ESP_LOGI(TAG, "    The 'No audio input detected' message means:");
    ESP_LOGI(TAG, "    1. LiveKit room may not be actively capturing audio");
    ESP_LOGI(TAG, "    2. Room connection might not be established yet");
    ESP_LOGI(TAG, "    3. Voice Activity Detection (VAD) might be filtering silence");
    ESP_LOGI(TAG, "    4. Audio data doesn't reach our monitoring hooks");
    
    ESP_LOGI(TAG, "  💡 To test audio:");
    ESP_LOGI(TAG, "    1. Ensure LiveKit room is connected");
    ESP_LOGI(TAG, "    2. Check room status in logs");
    ESP_LOGI(TAG, "    3. Speak loudly and clearly");
    ESP_LOGI(TAG, "    4. Look for LiveKit-specific audio logs");
    
    ESP_LOGI(TAG, "  🎤 Hardware Status: Should be checked by diagnostics");
    ESP_LOGI(TAG, "  🔊 Next check in 30 seconds");
}

void livekit_audio_monitor_test_capture(void)
{
    ESP_LOGI(TAG, "🧪 MANUAL AUDIO CAPTURE TEST:");
    
    esp_capture_handle_t capturer = media_get_capturer();
    if (!capturer) {
        ESP_LOGE(TAG, "  ❌ No capturer handle available");
        return;
    }
    
    ESP_LOGI(TAG, "  ✅ Capturer handle available");
    ESP_LOGI(TAG, "  💡 Manual capture test not implemented yet");
    ESP_LOGI(TAG, "  💡 Audio capture is controlled by LiveKit room state");
    ESP_LOGI(TAG, "  💡 Check LiveKit room connection logs for actual audio activity");
    
    // Print some useful information
    ESP_LOGI(TAG, "  📋 What to look for in logs:");
    ESP_LOGI(TAG, "    - 'Room connected' messages from LiveKit");
    ESP_LOGI(TAG, "    - Audio encoding/decoding activity");
    ESP_LOGI(TAG, "    - WebRTC connection status");
    ESP_LOGI(TAG, "    - Participant join/leave events");
    
    // Also run hardware diagnostics
    ESP_LOGI(TAG, "  🔧 Running hardware diagnostics...");
    media_run_audio_diagnostics();
}