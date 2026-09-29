#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    const char *name;
    const char *description;
    int (*command)(int argc, char **argv);
    esp_err_t (*diagnose)(void);
    bool implemented;
} lab_module_t;

extern const lab_module_t lab_system_module;
extern const lab_module_t lab_nfc_module;
extern const lab_module_t lab_cc1101_module;
extern const lab_module_t lab_ir_module;
extern const lab_module_t lab_gpio_module;
extern const lab_module_t lab_wifi_module;
extern const lab_module_t lab_ble_module;

const lab_module_t *const *lab_modules(size_t *count);
int lab_not_implemented(int argc, char **argv);
esp_err_t lab_nvs_init(void);
void lab_sanitize(char *text);
int lab_menu(int argc, char **argv);
esp_err_t lab_diagnose_all(void);