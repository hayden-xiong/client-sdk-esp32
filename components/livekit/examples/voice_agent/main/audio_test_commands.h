#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register audio test commands for ESP console
 * 
 * This will register the following commands:
 * - audio_diag: Run comprehensive audio diagnostics
 * - mic_test: Test microphone configuration  
 * - speaker_test: Test speaker configuration
 * - audio_stats: Print current audio statistics
 */
void register_audio_test_commands(void);

#ifdef __cplusplus
}
#endif