#include "core/module.h"

const lab_module_t lab_gpio_module = {
    .name = "gpio",
    .description = "GPIO placeholder; no pin writes",
    .command = lab_not_implemented,
    .diagnose = NULL,
    .implemented = false,
};