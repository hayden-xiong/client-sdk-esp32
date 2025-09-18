#include "esp_check.h"
#include "esp_log.h"
#include "codec_init.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"

#include "media.h"
#include "audio_monitor.h"
#include "audio_activity_monitor.h"
#include "audio_diagnostics.h"
#include "livekit_audio_monitor.h"

static const char *TAG = "media";

#define NULL_CHECK(pointer, message) \
    ESP_RETURN_ON_FALSE(pointer != NULL, -1, TAG, message)

typedef struct {
    esp_capture_sink_handle_t capturer_handle;
    esp_capture_audio_src_if_t *audio_source;
} capture_system_t;

typedef struct {
    audio_render_handle_t audio_renderer;
    av_render_handle_t av_renderer_handle;
} renderer_system_t;

static capture_system_t  capturer_system;
static renderer_system_t renderer_system;

static int build_capturer_system(void)
{
    esp_codec_dev_handle_t record_handle = get_record_handle();
    NULL_CHECK(record_handle, "Failed to get record handle");

    esp_capture_audio_aec_src_cfg_t codec_cfg = {
        .record_handle = record_handle,
        .channel = 4,
        .channel_mask = 1 | 2
    };
    capturer_system.audio_source = esp_capture_new_audio_aec_src(&codec_cfg);
    NULL_CHECK(capturer_system.audio_source, "Failed to create audio source");

    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capturer_system.audio_source
    };
    esp_capture_open(&cfg, &capturer_system.capturer_handle);
    NULL_CHECK(capturer_system.capturer_handle, "Failed to open capture system");
    return 0;
}

static int build_renderer_system(void)
{
    esp_codec_dev_handle_t render_device = get_playback_handle();
    NULL_CHECK(render_device, "Failed to get render device handle");

    i2s_render_cfg_t i2s_cfg = {
        .play_handle = render_device
    };
    renderer_system.audio_renderer = av_render_alloc_i2s_render(&i2s_cfg);
    NULL_CHECK(renderer_system.audio_renderer, "Failed to create I2S renderer");

    // Set initial speaker volume
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);

    av_render_cfg_t render_cfg = {
        .audio_render = renderer_system.audio_renderer,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    renderer_system.av_renderer_handle = av_render_open(&render_cfg);
    NULL_CHECK(renderer_system.av_renderer_handle, "Failed to create AV renderer");

    av_render_audio_frame_info_t frame_info = {
        .sample_rate = 16000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    av_render_set_fixed_frame_info(renderer_system.av_renderer_handle, &frame_info);

    return 0;
}

// Audio monitoring helper functions
static void monitor_audio_capture_data(const void *buffer, size_t size)
{
    if (buffer && size > 0) {
        // Monitor microphone input data
        // Assuming 16kHz, 1 channel, 16-bit samples based on the configuration
        audio_monitor_mic_input((const uint8_t *)buffer, size, 16000, 1, 16);
    }
}

static void monitor_audio_render_data(const void *buffer, size_t size)
{
    if (buffer && size > 0) {
        // Monitor speaker output data  
        // Assuming 16kHz, 2 channels, 16-bit samples based on the configuration
        audio_monitor_speaker_output((const uint8_t *)buffer, size, 16000, 2, 16);
    }
}

int media_init(void)
{
    // Initialize audio monitoring
    audio_monitor_config_t monitor_config = {
        .enable_mic_logging = true,
        .enable_speaker_logging = true,
        .log_interval_ms = 5000,      // Log every 5 seconds
        .volume_threshold = 100       // Minimum volume threshold for active logging
    };
    
    if (audio_monitor_init(&monitor_config) != 0) {
        ESP_LOGW(TAG, "Failed to initialize audio monitor, continuing without monitoring");
    }

    // Register default audio encoder and decoder
    esp_audio_enc_register_default();
    esp_audio_dec_register_default();

    // Build capturer and renderer systems
    build_capturer_system();
    build_renderer_system();
    
    // Start audio activity monitoring
    audio_activity_monitor_start();
    
    // Initialize LiveKit-specific audio monitoring
    livekit_audio_monitor_init();
    
    ESP_LOGI(TAG, "Audio monitoring initialized and ready");
    
    // Run initial audio diagnostics to check for common issues
    ESP_LOGI(TAG, "Running initial audio diagnostics...");
    audio_diagnostics_run_full_check();
    
    return 0;
}

esp_capture_handle_t media_get_capturer(void)
{
    return capturer_system.capturer_handle;
}

av_render_handle_t media_get_renderer(void)
{
    return renderer_system.av_renderer_handle;
}

void media_print_audio_stats(void)
{
    audio_stats_t mic_stats, speaker_stats;
    audio_monitor_get_mic_stats(&mic_stats);
    audio_monitor_get_speaker_stats(&speaker_stats);
    
    ESP_LOGI(TAG, "=== AUDIO STATISTICS ===");
    
    // Microphone statistics
    ESP_LOGI(TAG, "🎤 MICROPHONE:");
    if (mic_stats.total_frames > 0) {
        float active_percentage = (float)mic_stats.active_frames * 100.0f / mic_stats.total_frames;
        ESP_LOGI(TAG, "  Total frames: %lu", mic_stats.total_frames);
        ESP_LOGI(TAG, "  Active frames: %lu (%.1f%%)", mic_stats.active_frames, active_percentage);
        ESP_LOGI(TAG, "  Silent frames: %lu", mic_stats.silent_frames);
        ESP_LOGI(TAG, "  Peak volume: %lu", mic_stats.peak_volume);
        ESP_LOGI(TAG, "  Average volume: %lu", mic_stats.avg_volume);
        ESP_LOGI(TAG, "  Total bytes: %llu", mic_stats.total_bytes);
    } else {
        ESP_LOGI(TAG, "  No input detected");
    }
    
    // Speaker statistics
    ESP_LOGI(TAG, "🔊 SPEAKER:");
    if (speaker_stats.total_frames > 0) {
        float active_percentage = (float)speaker_stats.active_frames * 100.0f / speaker_stats.total_frames;
        ESP_LOGI(TAG, "  Total frames: %lu", speaker_stats.total_frames);
        ESP_LOGI(TAG, "  Active frames: %lu (%.1f%%)", speaker_stats.active_frames, active_percentage);
        ESP_LOGI(TAG, "  Silent frames: %lu", speaker_stats.silent_frames);
        ESP_LOGI(TAG, "  Peak volume: %lu", speaker_stats.peak_volume);
        ESP_LOGI(TAG, "  Average volume: %lu", speaker_stats.avg_volume);
        ESP_LOGI(TAG, "  Total bytes: %llu", speaker_stats.total_bytes);
    } else {
        ESP_LOGI(TAG, "  No output detected");
    }
    
    ESP_LOGI(TAG, "========================");
}

void media_log_audio_activity(void)
{
    ESP_LOGI(TAG, "=== MANUAL AUDIO ACTIVITY CHECK ===");
    
    // Log basic audio system status
    audio_activity_log_status();
    
    // Also trigger audio statistics if available
    media_print_audio_stats();
    
    // Manually call monitor functions to simulate data flow
    ESP_LOGI(TAG, "🎤 Simulating microphone data check...");
    monitor_audio_capture_data("test", 4); // Small test data
    
    ESP_LOGI(TAG, "🔊 Simulating speaker data check...");  
    monitor_audio_render_data("test", 4); // Small test data
    
    ESP_LOGI(TAG, "===================================");
}

void media_run_audio_diagnostics(void)
{
    audio_diagnostics_run_full_check();
}