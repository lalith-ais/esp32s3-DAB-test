idf_component_register(SRCS demo.c lcd.c  encoder.c logo.c test_screen.c 
                    INCLUDE_DIRS "."
                    REQUIRES   esp_driver_gpio esp_driver_pcnt esp_timer
                    )
