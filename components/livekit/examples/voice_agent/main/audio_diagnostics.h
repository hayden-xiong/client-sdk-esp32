#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run comprehensive audio system diagnostics
 * 
 * This function will check:
 * - Hardware initialization status
 * - Codec configuration
 * - Microphone mute status
 * - Audio capture system status
 * - LiveKit room connection status
 */
void audio_diagnostics_run_full_check(void);

/**
 * @brief Check microphone hardware status
 */
void audio_diagnostics_check_microphone(void);

/**
 * @brief Check speaker hardware status  
 */
void audio_diagnostics_check_speaker(void);

/**
 * @brief Check codec configuration and mute status
 */
void audio_diagnostics_check_codec_status(void);

/**
 * @brief Test audio capture system manually
 */
void audio_diagnostics_test_capture_system(void);

#ifdef __cplusplus
}
#endif