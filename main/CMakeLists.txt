idf_component_register(SRCS demo.c lcd.c touch.c encoder.c logo.c
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_ledc esp_driver_i2c esp_driver_gpio esp_driver_pcnt esp_timer
                    )
