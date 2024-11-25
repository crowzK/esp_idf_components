idf_component_register(SRCS "thingsboard.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls mqtt json esp_netif esp_wifi nvs_flash )
