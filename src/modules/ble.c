#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/module.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#define BLE_DEFAULT_SCAN_SECONDS 5
#define BLE_MAX_SCAN_SECONDS 30
#define BLE_SYNC_TIMEOUT_MS 5000

static const char *TAG = "ble";
static SemaphoreHandle_t synced;
static SemaphoreHandle_t scan_done;
static bool initialized;
static unsigned found;

static void on_sync(void)
{
    xSemaphoreGive(synced);
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "Host reset, reason %d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static esp_err_t ble_start(void)
{
    if (!initialized) {
        esp_err_t error = lab_nvs_init();
        if (error != ESP_OK) {
            return error;
        }
        if (synced == NULL) {
            synced = xSemaphoreCreateBinary();
            scan_done = xSemaphoreCreateBinary();
            if (synced == NULL || scan_done == NULL) {
                return ESP_ERR_NO_MEM;
            }
        }
        error = nimble_port_init();
        if (error != ESP_OK) {
            return error;
        }
        ble_hs_cfg.sync_cb = on_sync;
        ble_hs_cfg.reset_cb = on_reset;
        nimble_port_freertos_init(host_task);
        initialized = true;
    }
    if (!ble_hs_synced() && xSemaphoreTake(synced, pdMS_TO_TICKS(BLE_SYNC_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static int on_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        char name[32] = "";
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0 &&
            fields.name != NULL) {
            size_t length = fields.name_len < sizeof(name) - 1 ? fields.name_len : sizeof(name) - 1;
            memcpy(name, fields.name, length);
            lab_sanitize(name);
        }
        const uint8_t *address = event->disc.addr.val;
        printf("%02x:%02x:%02x:%02x:%02x:%02x rssi=%4d %s\n", address[5], address[4], address[3],
               address[2], address[1], address[0], event->disc.rssi, name);
        ++found;
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        xSemaphoreGive(scan_done);
    }
    return 0;
}

static int ble_scan(int seconds)
{
    uint8_t own_address_type;
    int status = ble_hs_id_infer_auto(0, &own_address_type);
    if (status != 0) {
        printf("ERR ble scan: address error %d\n", status);
        return ESP_FAIL;
    }
    // Passive scan: listens to advertisements without sending scan requests.
    struct ble_gap_disc_params params = {
        .passive = 1,
        .filter_duplicates = 1,
    };
    found = 0;
    xSemaphoreTake(scan_done, 0);
    status = ble_gap_disc(own_address_type, seconds * 1000, &params, on_gap_event, NULL);
    if (status != 0) {
        printf("ERR ble scan: start error %d\n", status);
        return ESP_FAIL;
    }
    if (xSemaphoreTake(scan_done, pdMS_TO_TICKS(seconds * 1000 + 2000)) != pdTRUE) {
        ble_gap_disc_cancel();
        puts("ERR ble scan: timed out");
        return ESP_ERR_TIMEOUT;
    }
    printf("OK %u devices\n", found);
    return 0;
}

static int ble_command(int argc, char **argv)
{
    static const char *usage = "ERR USAGE: ble scan [seconds 1..30]";
    if (argc < 2 || argc > 3 || strcmp(argv[1], "scan") != 0) {
        puts(usage);
        return ESP_ERR_INVALID_ARG;
    }
    int seconds = BLE_DEFAULT_SCAN_SECONDS;
    if (argc == 3) {
        char *end = NULL;
        long value = strtol(argv[2], &end, 10);
        if (*end != '\0' || value < 1 || value > BLE_MAX_SCAN_SECONDS) {
            puts(usage);
            return ESP_ERR_INVALID_ARG;
        }
        seconds = (int)value;
    }
    esp_err_t error = ble_start();
    if (error != ESP_OK) {
        printf("ERR ble start: %s\n", esp_err_to_name(error));
        return error;
    }
    return ble_scan(seconds);
}

static esp_err_t diagnose(void)
{
    printf("SKIP ble: radio stays off until the first ble command\n");
    return ESP_OK;
}

const lab_module_t lab_ble_module = {
    .name = "ble",
    .description = "Bluetooth LE: scan [seconds]",
    .command = ble_command,
    .diagnose = diagnose,
    .implemented = true,
};
