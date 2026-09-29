#include "core/module.h"
#include <stdio.h>
#include "esp_log.h"

static const lab_module_t *const modules[] = {
    &lab_system_module,
    &lab_nfc_module,
    &lab_cc1101_module,
    &lab_ir_module,
    &lab_gpio_module,
};

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
               modules[index]->implemented ? "ready" : "not implemented",
               modules[index]->description);
    }
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