#include <stdio.h>
#include <string.h>
#include "core/module.h"
#include "core/command_args.h"
#include "lab_config.h"
#include "pn532.h"
#include "pn532_driver_i2c.h"

static int command(int argc, char **argv)
{
    bool info = argc == 2 && strcmp(argv[1], "info") == 0;
    bool scan = argc == 2 && strcmp(argv[1], "scan") == 0;
    bool read = argc == 3 && strcmp(argv[1], "read") == 0;
    bool write = argc == 5 && strcmp(argv[1], "write") == 0 && strcmp(argv[4], "confirm") == 0;
    uint32_t page = 0;
    uint8_t bytes[4] = {0};
    size_t length = 0;
    if ((!info && !scan && !read && !write) ||
        ((read || write) && !lab_arg_u32(argv[2], 230, &page)) ||
        (write && (!lab_arg_hex(argv[3], bytes, sizeof(bytes), &length) || length != 4 || page < 4))) {
        puts("ERR USAGE: nfc info|scan|read <page>|write <page> <8 hex digits> confirm");
        return ESP_ERR_INVALID_ARG;
    }
    pn532_io_t io = {0};
    esp_err_t error = pn532_new_driver_i2c(LAB_PIN_I2C_SDA, LAB_PIN_I2C_SCL,
        LAB_PIN_PN532_RESET, GPIO_NUM_NC, I2C_NUM_0, &io);
    if (error != ESP_OK) return error;
    error = pn532_init(&io);
    uint32_t firmware = 0;
    if (error == ESP_OK) error = pn532_get_firmware_version(&io, &firmware);
    if (error == ESP_OK && ((firmware >> 24) & 255) != 0x32) error = ESP_ERR_NOT_SUPPORTED;
    if (error == ESP_OK && info) {
        printf("OK PN532 firmware=%u.%u support=0x%02x; I2C SDA8/SCL9 RESET5\n",
            (unsigned)((firmware >> 16) & 255), (unsigned)((firmware >> 8) & 255),
            (unsigned)(firmware & 255));
    }
    if (error == ESP_OK && !info) {
        error = pn532_set_passive_activation_retries(&io, 0);
        uint8_t uid[10] = {0}, uid_length = 0;
        if (error == ESP_OK) error = pn532_read_passive_target_id(&io,
            PN532_BRTY_ISO14443A_106KBPS, uid, &uid_length, 1000);
        if (error == ESP_OK && (uid_length == 0 || uid_length > sizeof(uid))) error = ESP_ERR_INVALID_SIZE;
        if (error == ESP_OK) {
            printf("OK ISO14443A UID=");
            for (unsigned index = 0; index < uid_length; ++index) printf("%02x", uid[index]);
            puts(" (identifier, not proof of authenticity)");
        }
        if (error == ESP_OK && (read || write)) {
            NTAG2XX_MODEL model = NTAG2XX_UNKNOWN;
            error = ntag2xx_get_model(&io, &model);
            uint32_t last_user = model == NTAG2XX_NTAG213 ? 39 :
                model == NTAG2XX_NTAG215 ? 129 : model == NTAG2XX_NTAG216 ? 225 : 0;
            if (error == ESP_OK && (!last_user || page > last_user)) {
                puts("ERR unsupported tag or non-user page; only NTAG213/215/216 user area");
                error = ESP_ERR_NOT_SUPPORTED;
            }
            uint8_t data[16] = {0};
            if (error == ESP_OK && write) error = ntag2xx_write_page(&io, (uint8_t)page, bytes);
            if (error == ESP_OK) error = ntag2xx_read_page(&io, (uint8_t)page, data, sizeof(data));
            if (error == ESP_OK && write && memcmp(bytes, data, sizeof(bytes)) != 0) error = ESP_FAIL;
            if (error == ESP_OK) {
                printf("OK page=%u data=%02x%02x%02x%02x%s\n", (unsigned)page,
                    data[0], data[1], data[2], data[3], write ? " verified" : "");
            }
            if (error != ESP_OK && write) puts("WARN write not verified; do not assume the tag is unchanged");
        }
    }
    pn532_release(&io);
    pn532_delete_driver(&io);
    if (error != ESP_OK) printf("ERR NFC: %s; check module, I2C mode, wiring and tag\n", esp_err_to_name(error));
    return error;
}

static esp_err_t diagnose(void)
{
    puts("SKIP nfc: PN532 driver available; hardware not probed during diagnostics");
    return ESP_OK;
}

const lab_module_t lab_nfc_module = {
    .name = "nfc", .description = "PN532: info|scan|read <page>|write <page> <hex> confirm",
    .command = command, .diagnose = diagnose, .implemented = true,
};