idf_component_register(
                    SRCS "src/fan_controller.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "driver" "esp_timer"
                    )
