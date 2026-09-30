#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "core/module.h"
#include "lab_config.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#define LAB_PIN_ENTRY(name, number) {#name, number},
static const struct {
    const char *name;
    int number;
} pins[] = { LAB_PIN_RESERVATIONS(LAB_PIN_ENTRY) };
#undef LAB_PIN_ENTRY

static bool pin_is_available(int number)
{
    return GPIO_IS_VALID_OUTPUT_GPIO(number) && number != 0 && number != 3 &&
           number != 19 && number != 20 && !(number >= 26 && number <= 46) &&
           number != 48;
}

static esp_err_t diagnose(void)
{
    bool healthy = true;
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    bool chip_ok = chip.model == CHIP_ESP32S3 && chip.cores == 2;
    printf("%s chip: ESP32-S3, two cores expected\n", chip_ok ? "OK" : "ERR");
    healthy &= chip_ok;

    uint32_t flash_bytes = 0;
    esp_err_t error = esp_flash_get_size(NULL, &flash_bytes);
    bool flash_ok = error == ESP_OK && flash_bytes == LAB_EXPECTED_FLASH_BYTES;
    printf("%s flash: detected=%" PRIu32 " expected=%u bytes (%s)\n",
           flash_ok ? "OK" : "ERR", flash_bytes, LAB_EXPECTED_FLASH_BYTES,
           esp_err_to_name(error));
    healthy &= flash_ok;

    size_t psram_bytes = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    bool psram_ok = psram_bytes == LAB_EXPECTED_PSRAM_BYTES;
    printf("%s PSRAM: detected=%u expected=%u bytes\n", psram_ok ? "OK" : "ERR",
           (unsigned)psram_bytes, LAB_EXPECTED_PSRAM_BYTES);
    healthy &= psram_ok;

    bool heap_ok = heap_caps_check_integrity_all(true);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    bool memory_ok = heap_ok && internal_free >= LAB_MIN_INTERNAL_HEAP_BYTES;
    printf("%s heap: integrity=%s internal_free=%u bytes\n", memory_ok ? "OK" : "ERR",
           heap_ok ? "pass" : "fail", (unsigned)internal_free);
    healthy &= memory_ok;

    bool pins_ok = true;
    for (size_t index = 0; index < sizeof(pins) / sizeof(pins[0]); ++index) {
        if (!pin_is_available(pins[index].number)) {
            printf("ERR pin reservation %s=%d unavailable\n", pins[index].name, pins[index].number);
            pins_ok = false;
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (pins[index].number == pins[previous].number) {
                printf("ERR duplicate pin: %s / %s\n", pins[index].name, pins[previous].name);
                pins_ok = false;
            }
        }
    }
    printf("%s pin reservations (configuration only; no electrical test)\n", pins_ok ? "OK" : "ERR");
    healthy &= pins_ok;
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_BROWNOUT || reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT) {
        ESP_LOGW("system", "Previous abnormal reset: %d; inspect boot logs", reason);
    }
    return healthy ? ESP_OK : ESP_FAIL;
}

static int system_command(int argc, char **argv)
{
    if (argc != 2) {
        puts("ERR USAGE: system info|diag|pins");
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(argv[1], "diag") == 0) {
        return lab_diagnose_all();
    }
    if (strcmp(argv[1], "info") == 0) {
        esp_chip_info_t chip;
        esp_chip_info(&chip);
        printf("OK %s %s\nIDF: %s\nTarget: %s\nCores: %d\nRevision: %d\n",
               LAB_NAME, LAB_VERSION, esp_get_idf_version(), CONFIG_IDF_TARGET, chip.cores, chip.revision);
        printf("Uptime: %" PRId64 " ms\nReset reason: %d\nFree heap: %" PRIu32
               " bytes\nMinimum free heap: %" PRIu32 " bytes\n",
               esp_timer_get_time() / 1000, esp_reset_reason(), esp_get_free_heap_size(),
               esp_get_minimum_free_heap_size());
        return 0;
    }
    if (strcmp(argv[1], "pins") == 0) {
        for (size_t index = 0; index < sizeof(pins) / sizeof(pins[0]); ++index) {
            printf("%-16s GPIO%d (reserved; runtime level not measured)\n", pins[index].name, pins[index].number);
        }
        puts("UART0=43/44 USB=19/20 JTAG=39..42 LED=38/48 BOOT=0/3/45/46 MEMORY=26..37");
        return 0;
    }
    puts("ERR USAGE: system info|diag|pins");
    return ESP_ERR_INVALID_ARG;
}

const lab_module_t lab_system_module = {
    .name = "system",
    .description = "System: info|diag|pins",
    .command = system_command,
    .diagnose = diagnose,
    .implemented = true,
};