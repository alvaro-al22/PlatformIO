#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "core/module.h"
#include "lab_config.h"
#include "onewire_bus.h"
#include "onewire_bus_impl_rmt.h"
#include "onewire_device.h"

static int command(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "scan") != 0) {
        puts("ERR USAGE: ibutton scan (1-Wire ROM IDs; GPIO6, external 4.7k pull-up to 3.3V)");
        return ESP_ERR_INVALID_ARG;
    }
    onewire_bus_handle_t bus = NULL;
    onewire_bus_config_t config = {.bus_gpio_num = LAB_PIN_IBUTTON};
    onewire_bus_rmt_config_t rmt = {.max_rx_bytes = 10};
    esp_err_t error = onewire_new_bus_rmt(&config, &rmt, &bus);
    if (error != ESP_OK) return error;
    onewire_device_iter_handle_t iterator = NULL;
    error = onewire_new_device_iter(bus, &iterator);
    unsigned count = 0;
    if (error == ESP_OK) {
        onewire_device_t device;
        while (count < 16 && (error = onewire_device_iter_get_next(iterator, &device)) == ESP_OK) {
            printf("OK ROM=%016" PRIx64 " family=%02x%s\n", device.address,
                   (unsigned)(device.address & 255), (device.address & 255) == 1 ? " DS1990A" : "");
            ++count;
        }
        esp_err_t cleanup = onewire_del_device_iter(iterator);
        if (cleanup != ESP_OK) error = cleanup;
        else if (error == ESP_ERR_NOT_FOUND && count) error = ESP_OK;
    }
    esp_err_t cleanup = onewire_bus_del(bus);
    printf("%s ibutton: %u devices (limit 16); ROM discovery only\n", count ? "OK" : "SKIP", count);
    return cleanup != ESP_OK ? cleanup : error;
}

static esp_err_t diagnose(void)
{
    puts("SKIP ibutton: no bus access during diagnostics; requires 1-Wire hardware");
    return ESP_OK;
}

const lab_module_t lab_ibutton_module = {
    .name = "ibutton", .description = "1-Wire: scan (ROM IDs, up to 16)",
    .command = command, .diagnose = diagnose, .implemented = true,
};