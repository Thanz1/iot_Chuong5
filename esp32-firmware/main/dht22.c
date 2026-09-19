#include "dht22.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "rom/ets_sys.h"

static gpio_num_t dht_gpio;

void dht22_init(gpio_num_t pin) {
    dht_gpio = pin;
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 1);
}

static int wait_for_level(int level, uint32_t timeout_us) {
    uint32_t start = esp_timer_get_time();
    while (gpio_get_level(dht_gpio) == level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return -1;
        }
    }
    return esp_timer_get_time() - start;
}

int dht22_read(float *temperature, float *humidity) {
    uint8_t data[5] = {0};

    // 1. Gửi xung Start (kéo xuống LOW ít nhất 1-2ms)
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    ets_delay_us(2000);
    gpio_set_level(dht_gpio, 1);
    ets_delay_us(30);

    // 2. Chuyển sang Input để nhận phản hồi từ DHT22
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);

    // Chờ DHT kéo LOW rồi HIGH phản hồi
    if (wait_for_level(0, 100) < 0) return -1;
    if (wait_for_level(1, 100) < 0) return -1;

    // 3. Đọc 40 bit dữ liệu (5 byte)
    for (int i = 0; i < 40; i++) {
        if (wait_for_level(0, 80) < 0) return -1;
        int duration = wait_for_level(1, 100);
        if (duration < 0) return -1;

        // Xung HIGH kéo dài > 40us là bit 1, ngắn hơn là bit 0
        if (duration > 40) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    // 4. Kiểm tra Checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        return -2; // Lỗi Checksum
    }

    // 5. Tính toán nhiệt độ & độ ẩm
    *humidity = (float)((data[0] << 8) | data[1]) / 10.0f;
    int16_t raw_temp = ((data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80) {
        raw_temp = -raw_temp;
    }
    *temperature = (float)raw_temp / 10.0f;

    return 0; // Thành công
}