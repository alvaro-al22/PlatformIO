idf_component_register(
    SRCS "main.c" "core/cli.c" "core/module.c"
         "modules/system.c" "modules/nfc.c" "modules/cc1101.c"
         "modules/ir.c" "modules/gpio.c" "modules/wifi.c" "modules/wifi_audit.c" "modules/ble.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES console esp_system esp_timer spi_flash esp_psram heap driver
             nvs_flash esp_wifi esp_netif esp_event bt
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)