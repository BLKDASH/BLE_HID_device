idf_component_register(SRCS "hardware_init.c"
                    INCLUDE_DIRS "include"
                    REQUIRES button led_strip ble_hidd 
                    PRIV_REQUIRES nvs_flash)
