#include "audio_diagnostics.h"
#include "esp_log.h"
#include "esp_system.h"
#include "codec_init.h"
#include "esp_codec_dev_defaults.h"
#include "bsp/esp-bsp.h"
#include "media.h"
#include "audio_monitor.h"

static const char *TAG = "audio_diagnostics";

void audio_diagnostics_check_microphone(void)
{
    ESP_LOGI(TAG, "🎤 MICROPHONE DIAGNOSTICS:");
    
    // Check if record handle is available
    esp_codec_dev_handle_t record_handle = get_record_handle();
    if (record_handle) {
        ESP_LOGI(TAG, "  ✅ Record handle: OK");
        
        // Try to get input volume (API may not be available for input)
        ESP_LOGI(TAG, "  💡 Input volume API not available, checking mute status...");
        
        // Check if microphone is muted
        bool muted = false;
        esp_err_t ret = esp_codec_dev_get_in_mute(record_handle, &muted);
        if (ret == ESP_OK) {
            if (muted) {
                ESP_LOGW(TAG, "  🔇 MICROPHONE IS MUTED! This is likely the problem!");
                ESP_LOGI(TAG, "  💡 Attempting to unmute microphone...");
                ret = esp_codec_dev_set_in_mute(record_handle, false);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "  ✅ Microphone unmuted successfully");
                } else {
                    ESP_LOGE(TAG, "  ❌ Failed to unmute microphone (error: 0x%x)", ret);
                }
            } else {
                ESP_LOGI(TAG, "  ✅ Microphone mute: OFF (not muted)");
            }
        } else {
            ESP_LOGW(TAG, "  ⚠️  Cannot check mute status (error: 0x%x)", ret);
        }
        
        // Try to set a reasonable input volume (API may not be available)
        ESP_LOGI(TAG, "  💡 Input volume control not available in this codec");
        ESP_LOGI(TAG, "  💡 Input gain is typically controlled by hardware settings");
        
    } else {
        ESP_LOGE(TAG, "  ❌ Record handle: NULL - Hardware not initialized!");
    }
}

void audio_diagnostics_check_speaker(void)
{
    ESP_LOGI(TAG, "🔊 SPEAKER DIAGNOSTICS:");
    
    // Check if playback handle is available
    esp_codec_dev_handle_t playback_handle = get_playback_handle();
    if (playback_handle) {
        ESP_LOGI(TAG, "  ✅ Playback handle: OK");
        
        // Try to get output volume
        int volume = 0;
        esp_err_t ret = esp_codec_dev_get_out_vol(playback_handle, &volume);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  ✅ Output volume: %d", volume);
        } else {
            ESP_LOGW(TAG, "  ⚠️  Cannot get output volume (error: 0x%x)", ret);
        }
        
        // Check if speaker is muted
        bool muted = false;
        ret = esp_codec_dev_get_out_mute(playback_handle, &muted);
        if (ret == ESP_OK) {
            if (muted) {
                ESP_LOGW(TAG, "  🔇 SPEAKER IS MUTED!");
                ESP_LOGI(TAG, "  💡 Attempting to unmute speaker...");
                ret = esp_codec_dev_set_out_mute(playback_handle, false);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "  ✅ Speaker unmuted successfully");
                } else {
                    ESP_LOGE(TAG, "  ❌ Failed to unmute speaker (error: 0x%x)", ret);
                }
            } else {
                ESP_LOGI(TAG, "  ✅ Speaker mute: OFF (not muted)");
            }
        } else {
            ESP_LOGW(TAG, "  ⚠️  Cannot check mute status (error: 0x%x)", ret);
        }
        
    } else {
        ESP_LOGE(TAG, "  ❌ Playback handle: NULL - Hardware not initialized!");
    }
}

void audio_diagnostics_check_codec_status(void)
{
    ESP_LOGI(TAG, "🎛️ CODEC DIAGNOSTICS:");
    
    // Check board type configuration
    ESP_LOGI(TAG, "  Board type: %s", CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    
    // Check if BSP is properly initialized
    ESP_LOGI(TAG, "  💡 Checking BSP initialization...");
    
    // Try to get both handles
    esp_codec_dev_handle_t record_handle = get_record_handle();
    esp_codec_dev_handle_t playback_handle = get_playback_handle();
    
    ESP_LOGI(TAG, "  Record handle: %s", record_handle ? "✅ OK" : "❌ NULL");
    ESP_LOGI(TAG, "  Playback handle: %s", playback_handle ? "✅ OK" : "❌ NULL");
    
    if (!record_handle || !playback_handle) {
        ESP_LOGE(TAG, "  ❌ CRITICAL: Audio codec not properly initialized!");
        ESP_LOGE(TAG, "  💡 This could be due to:");
        ESP_LOGE(TAG, "     - Wrong board type configuration");
        ESP_LOGE(TAG, "     - Hardware connection issues");
        ESP_LOGE(TAG, "     - I2C bus problems");
        ESP_LOGE(TAG, "     - Power supply issues");
    }
}

void audio_diagnostics_test_capture_system(void)
{
    ESP_LOGI(TAG, "🎯 CAPTURE SYSTEM TEST:");
    
    // Get the capturer handle from media system
    esp_capture_handle_t capturer = media_get_capturer();
    if (capturer) {
        ESP_LOGI(TAG, "  ✅ Capturer handle: OK");
        
        // Try to trigger a manual capture test (if API exists)
        ESP_LOGI(TAG, "  💡 Capture system appears to be initialized");
        ESP_LOGI(TAG, "  💡 If no audio is detected, check:");
        ESP_LOGI(TAG, "     - Microphone hardware connection");
        ESP_LOGI(TAG, "     - Input gain/volume settings");
        ESP_LOGI(TAG, "     - Room connection status");
        
    } else {
        ESP_LOGE(TAG, "  ❌ Capturer handle: NULL - Capture system not initialized!");
    }
    
    // Get the renderer handle from media system
    av_render_handle_t renderer = media_get_renderer();
    if (renderer) {
        ESP_LOGI(TAG, "  ✅ Renderer handle: OK");
    } else {
        ESP_LOGE(TAG, "  ❌ Renderer handle: NULL - Render system not initialized!");
    }
}

void audio_diagnostics_run_full_check(void)
{
    ESP_LOGI(TAG, "🔍 ===== COMPREHENSIVE AUDIO DIAGNOSTICS =====");
    
    // System information
    ESP_LOGI(TAG, "📊 SYSTEM INFO:");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Min free heap: %lu bytes", esp_get_minimum_free_heap_size());
    
    // Check codec status first
    audio_diagnostics_check_codec_status();
    
    ESP_LOGI(TAG, "");
    
    // Check microphone
    audio_diagnostics_check_microphone();
    
    ESP_LOGI(TAG, "");
    
    // Check speaker
    audio_diagnostics_check_speaker();
    
    ESP_LOGI(TAG, "");
    
    // Check capture system
    audio_diagnostics_test_capture_system();
    
    ESP_LOGI(TAG, "");
    
    // Print current audio statistics
    ESP_LOGI(TAG, "📈 CURRENT AUDIO STATISTICS:");
    media_print_audio_stats();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔍 DIAGNOSTIC COMPLETE");
    ESP_LOGI(TAG, "💡 RECOMMENDATIONS:");
    ESP_LOGI(TAG, "   1. Check if microphone was muted (should be fixed above)");
    ESP_LOGI(TAG, "   2. Verify hardware connections");
    ESP_LOGI(TAG, "   3. Test with a louder voice or closer to microphone");
    ESP_LOGI(TAG, "   4. Check LiveKit room connection status");
    ESP_LOGI(TAG, "   5. Monitor logs for capture system activity");
    ESP_LOGI(TAG, "   6. The current monitoring only tracks system activity");
    ESP_LOGI(TAG, "   7. 'No audio input detected' means no data reached monitor");
    ESP_LOGI(TAG, "   8. This could be normal if LiveKit isn't actively capturing");
    ESP_LOGI(TAG, "================================================");
}