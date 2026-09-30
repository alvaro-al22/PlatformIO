#include "core/module.h"
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const lab_module_t *const modules[] = {
    &lab_system_module,
    &lab_wifi_module,
    &lab_ble_module,
    &lab_nfc_module,
    &lab_cc1101_module,
    &lab_ir_module,
    &lab_gpio_module,
    &lab_ibutton_module,
    &lab_rfid_module,
    &lab_storage_module,
    &lab_usb_module,
};

esp_err_t lab_nvs_init(void)
{
    static bool ready;
    if (ready) {
        return ESP_OK;
    }
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE("nvs", "NVS needs manual recovery; stored data will NOT be erased automatically");
    }
    ready = error == ESP_OK;
    return error;
}

// Radio names come from nearby devices; strip control characters before printing.
void lab_sanitize(char *text)
{
    for (; *text != '\0'; ++text) {
        unsigned char character = (unsigned char)*text;
        if (character < 0x20 || character >= 0x7f) {
            *text = '?';
        }
    }
}

const lab_module_t *const *lab_modules(size_t *count)
{
    *count = sizeof(modules) / sizeof(modules[0]);
    return modules;
}

int lab_not_implemented(int argc, char **argv)
{
    (void)argc;
    printf("ERR NOT_IMPLEMENTED %s: not implemented; hardware not probed\n", argv[0]);
    return ESP_ERR_NOT_SUPPORTED;
}

int lab_menu(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        puts("ERR USAGE: menu");
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t index = 0; index < sizeof(modules) / sizeof(modules[0]); ++index) {
        printf("%-8s %-18s %s\n", modules[index]->name,
               modules[index]->implemented ? "software available" : "not implemented",
               modules[index]->description);
    }
    puts("NOTE software available does not mean hardware tested; see system diag and module help");
    return 0;
}

esp_err_t lab_diagnose_all(void)
{
    esp_err_t result = ESP_OK;
    for (size_t index = 0; index < sizeof(modules) / sizeof(modules[0]); ++index) {
        const lab_module_t *module = modules[index];
        if (module->diagnose == NULL) {
            printf("SKIP %s: not implemented; hardware not probed\n", module->name);
            continue;
        }
        esp_err_t error = module->diagnose();
        if (error != ESP_OK) {
            ESP_LOGE("diagnostics", "%s: %s", module->name, esp_err_to_name(error));
            result = error;
        }
    }
    puts(result == ESP_OK ? "OK implemented checks passed; SKIP is not PASS"
                          : "ERR diagnostics failed; console remains available");
    return result;
}