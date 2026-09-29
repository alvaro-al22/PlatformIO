#include "core/module.h"

const lab_module_t lab_ir_module = {
    .name = "ir",
    .description = "IR 38 kHz placeholder",
    .command = lab_not_implemented,
    .diagnose = NULL,
    .implemented = false,
};