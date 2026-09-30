#include "core/module.h"
#include <stdio.h>

int lab_cc1101_command(int argc, char **argv);

static esp_err_t diagnose(void)
{
    puts("SKIP cc1101: RadioLib driver available; RF hardware not probed");
    return ESP_OK;
}

const lab_module_t lab_cc1101_module = {
    .name = "cc1101",
    .description = "CC1101: init|status|rx|tx <hex> confirm|sleep",
    .command = lab_cc1101_command,
    .diagnose = diagnose,
    .implemented = true,
};