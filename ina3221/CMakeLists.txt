idf_component_register(
                    SRCS "src/ina3221.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "driver" "i2c_master"
                    )
