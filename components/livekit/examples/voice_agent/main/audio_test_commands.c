#include "audio_test_commands.h"
#include "audio_diagnostics.h"
#include "media.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

static const char *TAG = "audio_test";

// Console command for audio diagnostics
static int audio_diag_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Running audio diagnostics...");
    audio_diagnostics_run_full_check();
    return 0;
}

// Console command for microphone test
static int mic_test_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Testing microphone...");
    audio_diagnostics_check_microphone();
    return 0;
}

// Console command for speaker test
static int speaker_test_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Testing speaker...");
    audio_diagnostics_check_speaker();
    return 0;
}

// Console command for audio stats
static int audio_stats_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Printing audio statistics...");
    media_print_audio_stats();
    return 0;
}

void register_audio_test_commands(void)
{
    const esp_console_cmd_t audio_diag_cmd_def = {
        .command = "audio_diag",
        .help = "Run comprehensive audio diagnostics",
        .hint = NULL,
        .func = &audio_diag_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&audio_diag_cmd_def));

    const esp_console_cmd_t mic_test_cmd_def = {
        .command = "mic_test",
        .help = "Test microphone configuration",
        .hint = NULL,
        .func = &mic_test_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&mic_test_cmd_def));

    const esp_console_cmd_t speaker_test_cmd_def = {
        .command = "speaker_test",
        .help = "Test speaker configuration",
        .hint = NULL,
        .func = &speaker_test_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&speaker_test_cmd_def));

    const esp_console_cmd_t audio_stats_cmd_def = {
        .command = "audio_stats",
        .help = "Print current audio statistics",
        .hint = NULL,
        .func = &audio_stats_cmd,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&audio_stats_cmd_def));

    ESP_LOGI(TAG, "Audio test commands registered:");
    ESP_LOGI(TAG, "  audio_diag   - Run full audio diagnostics");
    ESP_LOGI(TAG, "  mic_test     - Test microphone");
    ESP_LOGI(TAG, "  speaker_test - Test speaker");
    ESP_LOGI(TAG, "  audio_stats  - Show audio statistics");
}