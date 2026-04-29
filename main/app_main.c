/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// necessary libraries for MQTT and OTA
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

// necessary libraries for OTA
#include "nvs.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include <sys/socket.h>

static const char *TAG = "MQTT_EXAMPLE";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static SemaphoreHandle_t s_msg_mutex = NULL;
static volatile bool received_command = false;
static uint8_t received_message[2048];

#define OTA_URL_SIZE    256
#define OTA_MAX_RETRIES   3
#define OTA_RETRY_DELAY_MS 5000

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "[HTTP] Error on transport layer");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "[HTTP] Connected to server");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "[HTTP] Request headers sent");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "[HTTP] Response header: %s: %s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "[HTTP] Data chunk received, len=%d bytes", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "[HTTP] Response finished");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "[HTTP] Disconnected from server");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "[HTTP] Redirect received");
        break;
    }
    return ESP_OK;
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "[MQTT] Event base=%s id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "=====================================");
        ESP_LOGI(TAG, "[MQTT] Connected to broker");
        ESP_LOGI(TAG, "[MQTT] Subscribing to: test/empa/message");
        msg_id = esp_mqtt_client_subscribe(client, "test/empa/message", 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "[MQTT] Subscribe failed");
        } else {
            ESP_LOGI(TAG, "[MQTT] Subscribe sent, msg_id=%d", msg_id);
        }
        ESP_LOGI(TAG, "=====================================");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "[MQTT] Disconnected from broker - will auto-reconnect");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "[MQTT] Subscription confirmed (msg_id=%d)", event->msg_id);
        ESP_LOGI(TAG, "[MQTT] Ready - waiting for commands...");
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGW(TAG, "[MQTT] Unsubscribed (msg_id=%d)", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "[MQTT] Publish acknowledged (msg_id=%d)", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "-------------------------------------");
        ESP_LOGI(TAG, "[MQTT] Message received");
        ESP_LOGI(TAG, "[MQTT] Topic  : %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "[MQTT] Payload: %.*s", event->data_len, event->data);
        ESP_LOGI(TAG, "[MQTT] Length : %d bytes", event->data_len);
        ESP_LOGI(TAG, "-------------------------------------");
        if (s_msg_mutex && xSemaphoreTake(s_msg_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            size_t copy_len = (size_t)event->data_len < sizeof(received_message) - 1
                              ? (size_t)event->data_len : sizeof(received_message) - 1;
            memcpy(received_message, event->data, copy_len);
            received_message[copy_len] = '\0';
            received_command = true;
            xSemaphoreGive(s_msg_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex, MQTT message dropped");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "[MQTT] Error event received");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(TAG, "[MQTT] Socket error: %s",
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGD(TAG, "[MQTT] Unhandled event id=%d", event->event_id);
        break;
    }
}

static void run_ota(const char *url)
{
    ESP_LOGI(TAG, "=====================================");
    ESP_LOGI(TAG, "[OTA] Starting OTA update");
    ESP_LOGI(TAG, "[OTA] URL: %s", url);
    ESP_LOGI(TAG, "=====================================");

    esp_http_client_config_t http_config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024 * 12,
        .buffer_size_tx = 1024 * 12,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    for (int attempt = 1; attempt <= OTA_MAX_RETRIES; attempt++)
    {
        ESP_LOGI(TAG, "[OTA] Attempt %d / %d  (free heap: %" PRIu32 " bytes)",
                 attempt, OTA_MAX_RETRIES, esp_get_free_heap_size());

        esp_https_ota_handle_t ota_handle = NULL;
        esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[OTA] esp_https_ota_begin failed (0x%x)", err);
            goto ota_retry;
        }

        int img_size = esp_https_ota_get_image_size(ota_handle);
        if (img_size > 0) {
            ESP_LOGI(TAG, "[OTA] Image size : %d bytes (%.1f KB)", img_size, img_size / 1024.0f);
        } else {
            ESP_LOGI(TAG, "[OTA] Image size : unknown - streaming...");
        }

        int last_logged_pct = -1;
        while (true) {
            err = esp_https_ota_perform(ota_handle);
            if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }
            int bytes_read = esp_https_ota_get_image_len_read(ota_handle);
            if (img_size > 0) {
                int pct = (bytes_read * 100) / img_size;
                if (pct - last_logged_pct >= 10) {
                    ESP_LOGI(TAG, "[OTA] Downloading... %3d%%  (%d / %d bytes)",
                             pct, bytes_read, img_size);
                    last_logged_pct = pct;
                }
            }
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[OTA] Perform failed (0x%x)", err);
            esp_https_ota_abort(ota_handle);
            goto ota_retry;
        }

        if (!esp_https_ota_is_complete_data_received(ota_handle)) {
            ESP_LOGE(TAG, "[OTA] Incomplete image received");
            esp_https_ota_abort(ota_handle);
            goto ota_retry;
        }

        ESP_LOGI(TAG, "[OTA] Download complete (%d bytes) - verifying image...",
                 esp_https_ota_get_image_len_read(ota_handle));
        err = esp_https_ota_finish(ota_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "=====================================");
            ESP_LOGI(TAG, "[OTA] SUCCESS - Rebooting in 1s...");
            ESP_LOGI(TAG, "=====================================");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "[OTA] Image validation FAILED - firmware may be corrupt");
        } else {
            ESP_LOGE(TAG, "[OTA] esp_https_ota_finish failed (0x%x)", err);
        }

ota_retry:
        if (attempt < OTA_MAX_RETRIES) {
            ESP_LOGW(TAG, "[OTA] Waiting %d ms before retry...", OTA_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(OTA_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "[OTA] All %d attempts failed - resuming MQTT", OTA_MAX_RETRIES);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io",
    };

    s_msg_mutex = xSemaphoreCreateMutex();
    configASSERT(s_msg_mutex);

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // Main APP loop
    cJSON *json = NULL;
    cJSON *type = NULL;
    cJSON *parsed_message = NULL;
    static uint8_t local_message[2048];
    bool cmd = false;
    while (1)
    {
        cmd = false;
        if (xSemaphoreTake(s_msg_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (received_command) {
                memcpy(local_message, received_message, sizeof(local_message));
                received_command = false;
                cmd = true;
            }
            xSemaphoreGive(s_msg_mutex);
        }
        if (cmd)
        {
            json = cJSON_Parse((char *)local_message);
            if (json == NULL)
            {
                ESP_LOGE(TAG, "Error in Parsing Json message");
                continue;
            }
            type = cJSON_GetObjectItemCaseSensitive(json, "type");
            if (!cJSON_IsNumber(type) || type->valueint < 0 || type->valueint > 1)
            {
                ESP_LOGE(TAG, "Type field missing or out of range");
                cJSON_Delete(json);
                continue;
            }
            parsed_message = cJSON_GetObjectItemCaseSensitive(json, "message");
            if (!cJSON_IsString(parsed_message) || (parsed_message->valuestring == NULL))
            {
                ESP_LOGE(TAG, "Message is not valid");
                cJSON_Delete(json);
                continue;
            }
            switch (type->valueint)
            {
            case 0: // Normal message job
                ESP_LOGI(TAG, "[APP] ---- Normal Message ----");
                ESP_LOGI(TAG, "[APP] Content  : %s", parsed_message->valuestring);
                ESP_LOGI(TAG, "[APP] Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
                cJSON_Delete(json);
                break;

            case 1: // OTA Job
                ESP_LOGI(TAG, "[APP] OTA command received - stopping MQTT client...");
                esp_mqtt_client_stop(client);
                run_ota(parsed_message->valuestring);
                ESP_LOGW(TAG, "[APP] OTA did not complete - resuming MQTT client...");
                cJSON_Delete(json);
                esp_mqtt_client_start(client);
                break;

            default:
                cJSON_Delete(json);
                break;
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void app_main(void)
{

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "=====================================");
    ESP_LOGI(TAG, "[APP] ===   ESP MQTT OTA FIRMWARE V2  ===");
    ESP_LOGI(TAG, "[APP] IDF version : %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free heap   : %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] Partition   : %s (offset=0x%08" PRIx32 ")",
             running->label, running->address);
    ESP_LOGI(TAG, "=====================================");

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
