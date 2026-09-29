#include "core/module.h"

const lab_module_t lab_nfc_module = {
    .name = "nfc",
    .description = "PN532 NFC/RFID placeholder",
    .command = lab_not_implemented,
    .diagnose = NULL,
    .implemented = false,
};