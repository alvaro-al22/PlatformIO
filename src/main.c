#include <stdio.h>
#include "core/cli.h"
#include "core/module.h"
#include "lab_config.h"
#include "esp_log.h"

void app_main(void)
{
    esp_log_level_set("*", LAB_LOG_LEVEL);
    printf("\n%s %s | USB-to-UART | %d baud\n", LAB_NAME, LAB_VERSION, LAB_BAUD_RATE);
    ESP_LOGI("main", "No peripheral buses, GPIO outputs or radios initialized");
    lab_diagnose_all();
    esp_err_t error = lab_cli_start();
    if (error != ESP_OK) {
        ESP_LOGE("main", "Console startup failed: %s; reset after checking logs",
                 esp_err_to_name(error));
    }
}