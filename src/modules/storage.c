#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "core/module.h"
#include "core/command_args.h"
#include "core/spi_bus.h"
#include "lab_config.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t *card;

static int command(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "mount") == 0) {
        if (card) return ESP_OK;
        esp_err_t error = lab_spi_start();
        if (error != ESP_OK) return error;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SPI2_HOST;
        host.max_freq_khz = 4000;
        sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
        device.host_id = SPI2_HOST;
        device.gpio_cs = LAB_PIN_SD_CS;
        esp_vfs_fat_sdmmc_mount_config_t mount = {.format_if_mount_failed = false,
            .max_files = 4, .allocation_unit_size = 16 * 1024};
        error = esp_vfs_fat_sdspi_mount("/sd", &host, &device, &mount, &card);
        if (error == ESP_OK) sdmmc_card_print_info(stdout, card);
        else { card = NULL; printf("ERR SD: %s; no automatic formatting\n", esp_err_to_name(error)); }
        return error;
    }
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        printf("OK SD %s\n", card ? "mounted at /sd" : "not mounted");
        return ESP_OK;
    }
    bool list = argc == 2 && strcmp(argv[1], "list") == 0;
    bool unmount = argc == 2 && strcmp(argv[1], "unmount") == 0;
    bool read = argc == 3 && strcmp(argv[1], "read") == 0 && lab_arg_name(argv[2]);
    bool write = argc == 5 && strcmp(argv[1], "write") == 0 && lab_arg_name(argv[2]) && strcmp(argv[4], "confirm") == 0;
    bool remove_file = argc == 4 && strcmp(argv[1], "remove") == 0 && lab_arg_name(argv[2]) && strcmp(argv[3], "confirm") == 0;
    if (!list && !unmount && !read && !write && !remove_file) {
        puts("ERR USAGE: sd mount|status|unmount|list|read <name>|write <name> <text> confirm|remove <name> confirm");
        return ESP_ERR_INVALID_ARG;
    }
    if (!card) { puts("ERR sd mount first; FAT formatted microSD required"); return ESP_ERR_INVALID_STATE; }
    if (unmount) {
        esp_err_t error = esp_vfs_fat_sdcard_unmount("/sd", card);
        if (error == ESP_OK) card = NULL;
        return error;
    }
    if (list) {
        DIR *directory = opendir("/sd");
        if (!directory) return ESP_FAIL;
        struct dirent *entry;
        unsigned count = 0;
        while (count < 64 && (entry = readdir(directory)) != NULL) {
            char name[sizeof(entry->d_name)];
            snprintf(name, sizeof(name), "%s", entry->d_name);
            lab_sanitize(name);
            puts(name);
            ++count;
        }
        closedir(directory);
        if (count == 64) puts("NOTE listing limit 64");
        return ESP_OK;
    }
    char path[32];
    snprintf(path, sizeof(path), "/sd/%s", argv[2]);
    if (remove_file) return remove(path) == 0 ? ESP_OK : ESP_FAIL;
    FILE *file = fopen(path, write ? "wx" : "r");
    if (!file) { puts("ERR file open failed; write creates a NEW file, never overwrites"); return ESP_FAIL; }
    esp_err_t error = ESP_OK;
    if (write) {
        size_t length = strlen(argv[3]);
        if (fwrite(argv[3], 1, length, file) != length || fflush(file) != 0) error = ESP_FAIL;
    } else {
        unsigned count = 0;
        int value;
        while (count < 4096 && (value = fgetc(file)) != EOF) {
            if (value >= 32 && value < 127) putchar(value);
            else printf("\\x%02x", (unsigned)value);
            ++count;
        }
        puts("\nNOTE read limit 4096 bytes; control bytes escaped");
        if (ferror(file)) error = ESP_FAIL;
    }
    if (fclose(file) != 0) error = ESP_FAIL;
    return error;
}

static esp_err_t diagnose(void)
{
    printf("SKIP sd: %s; media not tested by diagnostics\n", card ? "mounted" : "not mounted");
    return ESP_OK;
}

const lab_module_t lab_storage_module = {
    .name = "sd", .description = "microSD: mount|status|list|read|write|remove|unmount",
    .command = command, .diagnose = diagnose, .implemented = true,
};