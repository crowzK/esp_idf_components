idf_component_register(SRCS "task.cpp" "sw_timer.cpp" "work_queue.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_timer)
