idf_component_register(SRCS "thingsboard.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls mqtt esp_netif esp_wifi nvs_flash bblanchon__arduinojson app_update version esp-idf-cpp)
