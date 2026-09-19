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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "dht22.h"

static const char *TAG = "IOT_ESP32";

#define DEVICE_ID "esp32-001"
// TODO: Thay 192.168.1.100 bằng IP máy tính của bạn khi gõ lệnh ipconfig
#define MQTT_BROKER_URL "mqtt://192.168.1.4:1883"
#define DHT_PIN GPIO_NUM_15
#define LED_PIN GPIO_NUM_2

static bool led_state = false;
static esp_mqtt_client_handle_t mqtt_client;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        
        // Publish ONLINE status (retained = 1, qos = 1)
        char status_topic[100];
        sprintf(status_topic, "device/%s/status", DEVICE_ID);
        char status_payload[200];
        sprintf(status_payload, "{\"deviceId\":\"%s\",\"status\":\"ONLINE\",\"timestamp\":\"2026-09-17T08:30:00Z\"}", DEVICE_ID);
        esp_mqtt_client_publish(client, status_topic, status_payload, 0, 1, 1);

        // Subscribe to command topic
        char cmd_topic[100];
        sprintf(cmd_topic, "device/%s/command", DEVICE_ID);
        esp_mqtt_client_subscribe(client, cmd_topic, 1);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA received on topic: %.*s", event->topic_len, event->topic);
        if (event->data_len > 0) {
            cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
            if (root) {
                cJSON *cmd_id = cJSON_GetObjectItem(root, "commandId");
                cJSON *action = cJSON_GetObjectItem(root, "action");

                if (cJSON_IsString(action)) {
                    if (strcmp(action->valuestring, "LED_ON") == 0) {
                        led_state = true;
                        gpio_set_level(LED_PIN, 1);
                        ESP_LOGI(TAG, "Turned LED ON");
                    } else if (strcmp(action->valuestring, "LED_OFF") == 0) {
                        led_state = false;
                        gpio_set_level(LED_PIN, 0);
                        ESP_LOGI(TAG, "Turned LED OFF");
                    }

                    // Gửi bản tin ACK về lại Backend
                    if (cJSON_IsString(cmd_id)) {
                        char ack_topic[100];
                        sprintf(ack_topic, "device/%s/command/ack", DEVICE_ID);
                        char ack_payload[300];
                        sprintf(ack_payload, 
                                "{\"commandId\":\"%s\",\"deviceId\":\"%s\",\"action\":\"%s\",\"status\":\"ACKNOWLEDGED\",\"led\":%s,\"timestamp\":\"2026-09-17T08:31:01Z\"}",
                                cmd_id->valuestring, DEVICE_ID, action->valuestring, led_state ? "true" : "false");
                        
                        esp_mqtt_client_publish(client, ack_topic, ack_payload, 0, 1, 0);
                        ESP_LOGI(TAG, "Sent ACK for command: %s", cmd_id->valuestring);
                    }
                }
                cJSON_Delete(root);
            }
        }
        break;
        
    default:
        break;
    }
}

static void mqtt_app_start(void)
{
    char lwt_topic[100];
    sprintf(lwt_topic, "device/%s/status", DEVICE_ID);
    char lwt_payload[200];
    sprintf(lwt_payload, "{\"deviceId\":\"%s\",\"status\":\"OFFLINE\",\"timestamp\":\"2026-09-17T08:35:00Z\"}", DEVICE_ID);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .session.last_will = {
            .topic = lwt_topic,
            .msg = lwt_payload,
            .qos = 1,
            .retain = 1
        }
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void telemetry_task(void *pvParameters)
{
    float temp = 0.0f, hum = 0.0f;
    char topic[100];
    char payload[256];
    sprintf(topic, "device/%s/telemetry", DEVICE_ID);

    while (1) {
        int ret = dht22_read(&temp, &hum);
        if (ret == 0) {
            sprintf(payload, "{\"deviceId\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,\"led\":%s,\"timestamp\":\"2026-09-17T08:30:00Z\"}", 
                    DEVICE_ID, temp, hum, led_state ? "true" : "false");
            
            esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 0, 0);
            ESP_LOGI(TAG, "Published Telemetry: T=%.1f H=%.1f", temp, hum);
        } else {
            ESP_LOGW(TAG, "Failed to read DHT22 (code: %d)", ret);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    
    dht22_init(DHT_PIN);

    mqtt_app_start();

    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}