idf_component_register(SRC_DIRS src
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                       INCLUDE_DIRS src
                                src/fonts
                                src/images
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                       REQUIRES fatfs
                                spiffs
                                esp_adc_cal
                                ulp
                                nvs_flash
                                soc)