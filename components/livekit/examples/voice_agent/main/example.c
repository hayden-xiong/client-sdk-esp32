#include "esp_log.h"
#include "cJSON.h"
#include "bsp/esp-bsp.h"
#include "livekit.h"
#include "livekit_sandbox.h"
#include "media.h"
#include "board.h"
#include "example.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "livekit_example";

static livekit_room_handle_t room_handle;
static bool agent_joined = false;

/// Invoked when the room's connection state changes.
static void on_state_changed(livekit_connection_state_t state, void* ctx)
{
    ESP_LOGI(TAG, "Room state changed: %s", livekit_connection_state_str(state));

    livekit_failure_reason_t reason = livekit_room_get_failure_reason(room_handle);
    if (reason != LIVEKIT_FAILURE_REASON_NONE) {
        ESP_LOGE(TAG, "Failure reason: %s", livekit_failure_reason_str(reason));
    }
}

/// Invoked when participant information is received.
static void on_participant_info(const livekit_participant_info_t* info, void* ctx)
{
    if (info->kind != LIVEKIT_PARTICIPANT_KIND_AGENT) {
        // Only handle agent participants for this example.
        return;
    }
    bool joined = false;
    switch (info->state) {
        case LIVEKIT_PARTICIPANT_STATE_ACTIVE:       joined = true; break;
        case LIVEKIT_PARTICIPANT_STATE_DISCONNECTED: joined = false; break;
        default: return;
    }
    if (joined != agent_joined) {
        ESP_LOGI(TAG, "Agent has %s the room", joined ? "joined" : "left");
        agent_joined = joined;
    }
}

/// Invoked by a remote participant to set the state of an on-board LED.
static void set_led_state(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    if (invocation->payload == NULL) {
        livekit_rpc_return_error("Missing payload");
        return;
    }
    cJSON *root = cJSON_Parse(invocation->payload);
    if (!root) {
        livekit_rpc_return_error("Invalid JSON");
        return;
    }

    char* error = NULL;
    do {
        const cJSON *color_entry = cJSON_GetObjectItemCaseSensitive(root, "color");
        const cJSON *state_entry = cJSON_GetObjectItemCaseSensitive(root, "state");
        if (!cJSON_IsString(color_entry) || !cJSON_IsBool(state_entry)) {
            error = "Unexpected JSON format";
            break;
        }

        const char *color = color_entry->valuestring;

        bool state = cJSON_IsTrue(state_entry);

        bsp_led_t led;
        if (strncmp(color, "red", 3) == 0) {
            // TODO: there is a bug in the Korvo2 BSP which causes the LED pins to be swapped
            // (i.e., blue is mapped to red and red is mapped to blue): https://github.com/espressif/esp-bsp/pull/632
            led = BSP_LED_BLUE;
        } else if (strncmp(color, "blue", 4) == 0) {
            led = BSP_LED_RED;
        } else {
            error = "Unsupported color";
            break;
        }
        if (bsp_led_set(led, state) != ESP_OK) {
            error = "Failed to set LED state";
            break;
        }
    } while (0);

    if (!error) {
        livekit_rpc_return_ok(NULL);
    } else {
        livekit_rpc_return_error(error);
    }
    // Perform necessary cleanup after returning an RPC result.
    cJSON_Delete(root);
}

/// Invoked by a remote participant to get the current CPU temperature.
static void get_cpu_temp(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    float temp = board_get_temp();
    char temp_string[16];
    snprintf(temp_string, sizeof(temp_string), "%.2f", temp);
    livekit_rpc_return_ok(temp_string);
}

#ifdef CONFIG_LK_EXAMPLE_USE_DYNAMIC_TOKEN
// Structure to hold dynamic token response
typedef struct {
    char* server_url;
    char* room_name;
    char* participant_name;
    char* participant_token;
} dynamic_token_response_t;

// Structure to hold HTTP response data
typedef struct {
    char* buffer;
    int buffer_len;
    int data_len;
} http_response_data_t;

// HTTP event handler for collecting response data
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_data_t *response_data = (http_response_data_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Resize buffer if needed
            if (response_data->data_len + evt->data_len >= response_data->buffer_len) {
                int new_len = response_data->buffer_len * 2;
                if (new_len < response_data->data_len + evt->data_len + 1) {
                    new_len = response_data->data_len + evt->data_len + 1;
                }
                char* new_buffer = realloc(response_data->buffer, new_len);
                if (new_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to reallocate response buffer");
                    return ESP_FAIL;
                }
                response_data->buffer = new_buffer;
                response_data->buffer_len = new_len;
            }
            
            // Copy data to buffer
            memcpy(response_data->buffer + response_data->data_len, evt->data, evt->data_len);
            response_data->data_len += evt->data_len;
            response_data->buffer[response_data->data_len] = '\0';
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

// Function to fetch dynamic token from API
static bool fetch_dynamic_token(dynamic_token_response_t* result)
{
    ESP_LOGI(TAG, "=== Starting Dynamic Token Fetch ===");
    ESP_LOGI(TAG, "Target API: %s", CONFIG_LK_EXAMPLE_API_URL);
    ESP_LOGI(TAG, "Room: %s, Participant: %s", CONFIG_LK_EXAMPLE_DYNAMIC_ROOM_NAME, CONFIG_LK_EXAMPLE_DYNAMIC_PARTICIPANT_NAME);
    ESP_LOGI(TAG, "=====================================");
    
    bool success = false;
    esp_http_client_handle_t client = NULL;
    http_response_data_t response_data = {0};
    cJSON *json_request = NULL;
    cJSON *json_response = NULL;
    char *request_body = NULL;
    
    // Initialize response buffer
    response_data.buffer_len = 1024;
    response_data.buffer = malloc(response_data.buffer_len);
    if (response_data.buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return false;
    }
    response_data.data_len = 0;
    
    // Create JSON request body
    json_request = cJSON_CreateObject();
    if (json_request == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON request");
        goto cleanup;
    }
    
    cJSON_AddStringToObject(json_request, "room_name", CONFIG_LK_EXAMPLE_DYNAMIC_ROOM_NAME);
    cJSON_AddStringToObject(json_request, "participant_name", CONFIG_LK_EXAMPLE_DYNAMIC_PARTICIPANT_NAME);
    
    // Add room_config with agents
    cJSON *room_config = cJSON_CreateObject();
    cJSON *agents_array = cJSON_CreateArray();
    cJSON *agent = cJSON_CreateObject();
    
    cJSON_AddStringToObject(agent, "agent_name", CONFIG_LK_EXAMPLE_AGENT_NAME);
    cJSON_AddStringToObject(agent, "metadata", CONFIG_LK_EXAMPLE_AGENT_METADATA);
    cJSON_AddItemToArray(agents_array, agent);
    cJSON_AddItemToObject(room_config, "agents", agents_array);
    cJSON_AddItemToObject(json_request, "room_config", room_config);
    
    request_body = cJSON_Print(json_request);
    if (request_body == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON request");
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "=== API Request Details ===");
    ESP_LOGI(TAG, "API URL: %s", CONFIG_LK_EXAMPLE_API_URL);
    ESP_LOGI(TAG, "Sandbox ID Header: %s", CONFIG_LK_EXAMPLE_SANDBOX_ID_HEADER);
    ESP_LOGI(TAG, "Request body: %s", request_body);
    ESP_LOGI(TAG, "===========================");
    
    // Configure HTTP client
    ESP_LOGI(TAG, "Configuring HTTPS client with certificate bundle...");
    esp_http_client_config_t config = {
        .url = CONFIG_LK_EXAMPLE_API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &response_data,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Use certificate bundle
    };
    
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    ESP_LOGI(TAG, "HTTP client initialized successfully");
    
    // Set headers
    ESP_LOGI(TAG, "Setting HTTP headers...");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Sandbox-ID", CONFIG_LK_EXAMPLE_SANDBOX_ID_HEADER);
    
    // Set request body
    ESP_LOGI(TAG, "Setting request body (%d bytes)...", (int)strlen(request_body));
    esp_http_client_set_post_field(client, request_body, strlen(request_body));
    
    // Perform HTTP request
    ESP_LOGI(TAG, "Performing HTTPS request...");
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Check HTTP status code
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    ESP_LOGI(TAG, "=== HTTP Response Details ===");
    ESP_LOGI(TAG, "Status Code: %d", status_code);
    ESP_LOGI(TAG, "Content Length: %d", content_length);
    ESP_LOGI(TAG, "Response Length: %d", response_data.data_len);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
        ESP_LOGE(TAG, "Error Response: %s", response_data.buffer);
        ESP_LOGI(TAG, "=============================");
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Success Response: %s", response_data.buffer);
    ESP_LOGI(TAG, "============================");
    
    // Parse JSON response
    ESP_LOGI(TAG, "Parsing JSON response...");
    json_response = cJSON_Parse(response_data.buffer);
    if (json_response == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        goto cleanup;
    }
    ESP_LOGI(TAG, "JSON parsed successfully");
    
    // Extract fields from response
    ESP_LOGI(TAG, "Extracting fields from JSON response...");
    cJSON *server_url = cJSON_GetObjectItemCaseSensitive(json_response, "serverUrl");
    cJSON *room_name = cJSON_GetObjectItemCaseSensitive(json_response, "roomName");
    cJSON *participant_name = cJSON_GetObjectItemCaseSensitive(json_response, "participantName");
    cJSON *participant_token = cJSON_GetObjectItemCaseSensitive(json_response, "participantToken");
    
    ESP_LOGI(TAG, "Field extraction results:");
    ESP_LOGI(TAG, "  serverUrl: %s", cJSON_IsString(server_url) ? "✓ Found" : "✗ Missing/Invalid");
    ESP_LOGI(TAG, "  roomName: %s", cJSON_IsString(room_name) ? "✓ Found" : "✗ Missing/Invalid");
    ESP_LOGI(TAG, "  participantName: %s", cJSON_IsString(participant_name) ? "✓ Found" : "✗ Missing/Invalid");
    ESP_LOGI(TAG, "  participantToken: %s", cJSON_IsString(participant_token) ? "✓ Found" : "✗ Missing/Invalid");
    
    if (!cJSON_IsString(server_url) || !cJSON_IsString(room_name) ||
        !cJSON_IsString(participant_name) || !cJSON_IsString(participant_token)) {
        ESP_LOGE(TAG, "Invalid JSON response format - missing required fields");
        goto cleanup;
    }
    
    // Allocate and copy strings
    ESP_LOGI(TAG, "Allocating memory and copying response strings...");
    result->server_url = strdup(server_url->valuestring);
    result->room_name = strdup(room_name->valuestring);
    result->participant_name = strdup(participant_name->valuestring);
    result->participant_token = strdup(participant_token->valuestring);
    
    if (result->server_url == NULL || result->room_name == NULL ||
        result->participant_name == NULL || result->participant_token == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response strings");
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "=== Token Fetch Success ===");
    ESP_LOGI(TAG, "Successfully fetched dynamic token from API");
    ESP_LOGI(TAG, "Server URL: %s", result->server_url);
    ESP_LOGI(TAG, "Room Name: %s", result->room_name);
    ESP_LOGI(TAG, "Participant Name: %s", result->participant_name);
    ESP_LOGI(TAG, "Token Preview: %.30s...", result->participant_token);
    ESP_LOGI(TAG, "Token Full Length: %d characters", (int)strlen(result->participant_token));
    ESP_LOGI(TAG, "===========================");
    
    success = true;
    
cleanup:
    if (!success && result) {
        // Clean up partially allocated result on failure
        free(result->server_url);
        free(result->room_name);
        free(result->participant_name);
        free(result->participant_token);
        memset(result, 0, sizeof(dynamic_token_response_t));
    }
    
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (response_data.buffer) {
        free(response_data.buffer);
    }
    if (json_request) {
        cJSON_Delete(json_request);
    }
    if (json_response) {
        cJSON_Delete(json_response);
    }
    if (request_body) {
        free(request_body);
    }
    
    return success;
}

// Function to free dynamic token response
static void free_dynamic_token_response(dynamic_token_response_t* response)
{
    if (response) {
        free(response->server_url);
        free(response->room_name);
        free(response->participant_name);
        free(response->participant_token);
        memset(response, 0, sizeof(dynamic_token_response_t));
    }
}
#endif // CONFIG_LK_EXAMPLE_USE_DYNAMIC_TOKEN

void join_room()
{
    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return;
    }

    livekit_room_options_t room_options = {
        .publish = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .audio_encode = {
                .codec = LIVEKIT_AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channel_count = 1
            },
            .capturer = media_get_capturer()
        },
        .subscribe = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .renderer = media_get_renderer()
        },
        .on_state_changed = on_state_changed,
        .on_participant_info = on_participant_info
    };
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return;
    }

    // Register RPC handlers so they can be invoked by remote participants.
    livekit_room_rpc_register(room_handle, "set_led_state", set_led_state);
    livekit_room_rpc_register(room_handle, "get_cpu_temp", get_cpu_temp);

    livekit_err_t connect_res;
#ifdef CONFIG_LK_EXAMPLE_USE_SANDBOX
    // Option A: Sandbox token server.
    livekit_sandbox_res_t res = {};
    livekit_sandbox_options_t gen_options = {
        .sandbox_id = CONFIG_LK_EXAMPLE_SANDBOX_ID,
        .room_name = CONFIG_LK_EXAMPLE_ROOM_NAME,
        .participant_name = CONFIG_LK_EXAMPLE_PARTICIPANT_NAME
    };
    if (!livekit_sandbox_generate(&gen_options, &res)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return;
    }
    connect_res = livekit_room_connect(room_handle, res.server_url, res.token);
    livekit_sandbox_res_free(&res);
#elif defined(CONFIG_LK_EXAMPLE_USE_DYNAMIC_TOKEN)
    // Option B: Dynamic token from API.
    ESP_LOGI(TAG, "Preparing to fetch dynamic token from API...");
    
    // Wait for time synchronization (required for SSL certificate validation)
    ESP_LOGI(TAG, "Checking time synchronization status...");
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry_count = 0;
    const int max_retry = 10;
    
    // Get initial time first
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Initial time check: %04d-%02d-%02d %02d:%02d:%02d (timestamp: %ld)", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (long)now);
    
    // Check if time appears to be from Unix epoch (indicates not synchronized)
    // Unix epoch time starts at 1970-01-01, so anything before 2020 is likely unsynchronized
    const time_t min_valid_timestamp = 1577836800; // 2020-01-01 00:00:00 UTC
    
    if (now >= min_valid_timestamp) {
        ESP_LOGI(TAG, "Time appears to be synchronized, no waiting needed");
    } else {
        ESP_LOGI(TAG, "Time appears to be from Unix epoch, waiting for SNTP synchronization...");
        
        while (now < min_valid_timestamp && retry_count < max_retry) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry_count + 1, max_retry);
            ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d (timestamp: %ld)", 
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (long)now);
            
            vTaskDelay(pdMS_TO_TICKS(2000));
            time(&now);
            localtime_r(&now, &timeinfo);
            retry_count++;
        }
    }
    
    // Final status check
    if (now < min_valid_timestamp) {
        ESP_LOGW(TAG, "Time synchronization timeout after %d attempts, continuing anyway...", max_retry);
        ESP_LOGW(TAG, "Final time: %04d-%02d-%02d %02d:%02d:%02d (timestamp: %ld)", 
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (long)now);
        ESP_LOGW(TAG, "SSL certificate validation may fail due to incorrect time!");
        ESP_LOGW(TAG, "Please check network connection and NTP server availability");
    } else {
        ESP_LOGI(TAG, "Time synchronized successfully: %04d-%02d-%02d %02d:%02d:%02d (timestamp: %ld)", 
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (long)now);
    }
    
    ESP_LOGI(TAG, "Starting dynamic token fetch...");
    dynamic_token_response_t dynamic_res = {};
    if (!fetch_dynamic_token(&dynamic_res)) {
        ESP_LOGE(TAG, "Failed to fetch dynamic token from API");
        return;
    }
    
    ESP_LOGI(TAG, "=== Dynamic Token Connection Details ===");
    ESP_LOGI(TAG, "Server URL: %s", dynamic_res.server_url);
    ESP_LOGI(TAG, "Room Name: %s", dynamic_res.room_name);
    ESP_LOGI(TAG, "Participant Name: %s", dynamic_res.participant_name);
    ESP_LOGI(TAG, "Participant Token: %.50s...", dynamic_res.participant_token); // Only show first 50 chars for security
    ESP_LOGI(TAG, "Token Length: %d", (int)strlen(dynamic_res.participant_token));
    ESP_LOGI(TAG, "========================================");
    
    ESP_LOGI(TAG, "Connecting to room with dynamic token...");
    connect_res = livekit_room_connect(room_handle, dynamic_res.server_url, dynamic_res.participant_token);
    free_dynamic_token_response(&dynamic_res);
#else
    // Option C: Pre-generated token.
    ESP_LOGI(TAG, "Using pre-generated token");
    connect_res = livekit_room_connect(
        room_handle,
        CONFIG_LK_EXAMPLE_SERVER_URL,
        CONFIG_LK_EXAMPLE_TOKEN);
#endif

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
    }
}

void leave_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return;
    }
    if (livekit_room_close(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to leave room");
    }
    if (livekit_room_destroy(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return;
    }
    room_handle = NULL;
}