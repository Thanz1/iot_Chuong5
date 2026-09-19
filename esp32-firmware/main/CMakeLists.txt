idf_component_register(SRCS "main.c" "dht22.c"
                       INCLUDE_DIRS "."
                       REQUIRES protocol_examples_common esp_wifi nvs_flash mqtt json esp_driver_gpio esp_timer)