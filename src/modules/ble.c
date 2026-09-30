#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "core/module.h"
#include "core/command_args.h"
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
static SemaphoreHandle_t link_done, gatt_done;
static atomic_uint connection = BLE_HS_CONN_HANDLE_NONE;
static atomic_bool connecting, wanted_connection, gatt_busy, link_closing;
static atomic_int central_result;
static unsigned gatt_count;

static void finish_gatt(int status)
{
    atomic_store(&central_result, status == BLE_HS_EDONE ? 0 : status);
    atomic_store(&gatt_busy, false);
    xSemaphoreGive(gatt_done);
}

static int central_gap(struct ble_gap_event *event, void *context)
{
    (void)context;
    if (event->type == BLE_GAP_EVENT_CONNECT) {
        atomic_store(&connecting, false);
        atomic_store(&central_result, event->connect.status);
        if (event->connect.status == 0) {
            atomic_store(&connection, event->connect.conn_handle);
            if (!atomic_load(&wanted_connection)) {
                atomic_store(&link_closing, true);
                ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        }
        xSemaphoreGive(link_done);
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        if (atomic_load(&connection) == event->disconnect.conn.conn_handle) {
            atomic_store(&connection, BLE_HS_CONN_HANDLE_NONE);
            atomic_store(&link_closing, false);
            if (atomic_load(&gatt_busy)) finish_gatt(BLE_HS_ENOTCONN);
        }
        xSemaphoreGive(link_done);
    }
    return 0;
}

static int on_service(uint16_t handle, const struct ble_gatt_error *error,
                      const struct ble_gatt_svc *service, void *context)
{
    (void)handle; (void)context;
    if (error->status != 0) { finish_gatt(error->status); return 0; }
    char uuid[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&service->uuid.u, uuid);
    printf("GATT service start=%u end=%u UUID=%s\n", service->start_handle, service->end_handle, uuid);
    if (++gatt_count >= 64) { puts("NOTE discovery limit 64"); finish_gatt(0); return 1; }
    return 0;
}

static int on_characteristic(uint16_t handle, const struct ble_gatt_error *error,
                             const struct ble_gatt_chr *characteristic, void *context)
{
    (void)handle; (void)context;
    if (error->status != 0) { finish_gatt(error->status); return 0; }
    char uuid[BLE_UUID_STR_LEN];
    ble_uuid_to_str(&characteristic->uuid.u, uuid);
    printf("GATT value_handle=%u properties=0x%02x UUID=%s\n",
        characteristic->val_handle, characteristic->properties, uuid);
    if (++gatt_count >= 64) { puts("NOTE discovery limit 64"); finish_gatt(0); return 1; }
    return 0;
}

static int on_read(uint16_t handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attribute, void *context)
{
    (void)handle; (void)context;
    if (!error->status && attribute && attribute->om) {
        uint8_t bytes[64];
        size_t length = OS_MBUF_PKTLEN(attribute->om);
        if (length > sizeof(bytes)) length = sizeof(bytes);
        if (os_mbuf_copydata(attribute->om, 0, length, bytes) != 0) { finish_gatt(BLE_HS_EUNKNOWN); return 0; }
        printf("GATT value=");
        for (size_t index = 0; index < length; ++index) printf("%02x", bytes[index]);
        puts(" (single ATT read, display max64bytes)");
    }
    finish_gatt(error->status);
    return 0;
}

static int on_write(uint16_t handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attribute, void *context)
{
    (void)handle; (void)attribute; (void)context;
    finish_gatt(error->status);
    return 0;
}

static void on_sync(void)
{
    xSemaphoreGive(synced);
}

static void on_reset(int reason)
{
    atomic_store(&connecting, false);
    atomic_store(&connection, BLE_HS_CONN_HANDLE_NONE);
    atomic_store(&link_closing, false);
    if (link_done) xSemaphoreGive(link_done);
    if (gatt_done && atomic_load(&gatt_busy)) finish_gatt(BLE_HS_ENOTCONN);
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
        if (!synced) synced = xSemaphoreCreateBinary();
        if (!scan_done) scan_done = xSemaphoreCreateBinary();
        if (!link_done) link_done = xSemaphoreCreateBinary();
        if (!gatt_done) gatt_done = xSemaphoreCreateBinary();
        if (!synced || !scan_done || !link_done || !gatt_done) return ESP_ERR_NO_MEM;
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
         printf("%02x:%02x:%02x:%02x:%02x:%02x type=%u rssi=%4d %s\n", address[5], address[4], address[3],
             address[2], address[1], address[0], event->disc.addr.type, event->disc.rssi, name);
        ++found;
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        xSemaphoreGive(scan_done);
    }
    return 0;
}

static int ble_scan(int seconds)
{
    if (ble_gap_disc_active() || atomic_load(&connecting)) return ESP_ERR_INVALID_STATE;
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
        xSemaphoreTake(scan_done, pdMS_TO_TICKS(1000));
        puts("ERR ble scan: timed out");
        return ESP_ERR_TIMEOUT;
    }
    printf("OK %u devices\n", found);
    return 0;
}

static int central_command(int argc, char **argv)
{
    if (atomic_load(&link_closing)) {
        puts("ERR BLE disconnect pending; wait for closure or reset device");
        return ESP_ERR_INVALID_STATE;
    }
    if (strcmp(argv[1], "connect") == 0) {
        ble_addr_t peer = {0};
        if (argc != 4 || strlen(argv[2]) != 17 ||
            (strcmp(argv[3], "public") != 0 && strcmp(argv[3], "random") != 0)) return ESP_ERR_INVALID_ARG;
        peer.type = strcmp(argv[3], "random") == 0 ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
        for (size_t index = 0; index < 6; ++index) {
            int high = lab_hex_digit(argv[2][index * 3]), low = lab_hex_digit(argv[2][index * 3 + 1]);
            if (high < 0 || low < 0 || (index < 5 && argv[2][index * 3 + 2] != ':')) return ESP_ERR_INVALID_ARG;
            peer.val[5 - index] = (uint8_t)((high << 4) | low);
        }
        if (atomic_load(&connection) != BLE_HS_CONN_HANDLE_NONE || atomic_load(&connecting) || ble_gap_disc_active()) return ESP_ERR_INVALID_STATE;
        uint8_t address_type;
        int status = ble_hs_id_infer_auto(0, &address_type);
        if (status) return ESP_FAIL;
        xSemaphoreTake(link_done, 0);
        atomic_store(&connecting, true);
        atomic_store(&wanted_connection, true);
        status = ble_gap_connect(address_type, &peer, 10000, NULL, central_gap, NULL);
        if (status) { atomic_store(&connecting, false); return ESP_FAIL; }
        if (xSemaphoreTake(link_done, pdMS_TO_TICKS(12000)) != pdTRUE) {
            atomic_store(&wanted_connection, false);
            ble_gap_conn_cancel();
            xSemaphoreTake(link_done, pdMS_TO_TICKS(1000));
            uint16_t late_handle = atomic_load(&connection);
            if (late_handle != BLE_HS_CONN_HANDLE_NONE) {
                atomic_store(&link_closing, true);
                ble_gap_terminate(late_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
            return ESP_ERR_TIMEOUT;
        }
        printf("BLE connect status=%d handle=%u\n", atomic_load(&central_result), atomic_load(&connection));
        return atomic_load(&connection) != BLE_HS_CONN_HANDLE_NONE ? ESP_OK : ESP_FAIL;
    }
    uint16_t handle = atomic_load(&connection);
    if (handle == BLE_HS_CONN_HANDLE_NONE) { puts("ERR ble connect first"); return ESP_ERR_INVALID_STATE; }
    if (argc == 2 && strcmp(argv[1], "disconnect") == 0) {
        xSemaphoreTake(link_done, 0);
        atomic_store(&link_closing, true);
        int status = ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
        if (status) { atomic_store(&link_closing, false); return ESP_FAIL; }
        if (xSemaphoreTake(link_done, pdMS_TO_TICKS(3000)) != pdTRUE) return ESP_ERR_TIMEOUT;
        return atomic_load(&connection) == BLE_HS_CONN_HANDLE_NONE ? ESP_OK : ESP_FAIL;
    }
    bool services = argc == 2 && strcmp(argv[1], "services") == 0;
    bool characteristics = argc == 4 && strcmp(argv[1], "chars") == 0;
    bool read = argc == 3 && strcmp(argv[1], "read") == 0;
    bool write = argc == 5 && strcmp(argv[1], "write") == 0 && strcmp(argv[4], "confirm") == 0;
    uint32_t first = 0, last = 0;
    uint8_t bytes[20];
    size_t length = 0;
    if ((!services && !characteristics && !read && !write) || ((characteristics || read || write) &&
        (!lab_arg_u32(argv[2], 65535, &first) || !first)) ||
        (characteristics && (!lab_arg_u32(argv[3], 65535, &last) || last < first)) ||
        (write && !lab_arg_hex(argv[3], bytes, sizeof(bytes), &length))) return ESP_ERR_INVALID_ARG;
    if (atomic_load(&gatt_busy)) return ESP_ERR_INVALID_STATE;
    gatt_count = 0;
    xSemaphoreTake(gatt_done, 0);
    atomic_store(&gatt_busy, true);
    int status = services ? ble_gattc_disc_all_svcs(handle, on_service, NULL) :
        characteristics ? ble_gattc_disc_all_chrs(handle, first, last, on_characteristic, NULL) :
        read ? ble_gattc_read(handle, first, on_read, NULL) :
        ble_gattc_write_flat(handle, first, bytes, length, on_write, NULL);
    if (status) { atomic_store(&gatt_busy, false); return ESP_FAIL; }
    if (xSemaphoreTake(gatt_done, pdMS_TO_TICKS(10000)) != pdTRUE) {
        atomic_store(&link_closing, true);
        ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
        puts("ERR GATT timeout; disconnect requested; a write may already have reached the device");
        return ESP_ERR_TIMEOUT;
    }
    status = atomic_load(&central_result);
    printf("%s GATT status=%d (remote permissions/security apply)\n", status ? "ERR" : "OK", status);
    return status ? ESP_FAIL : ESP_OK;
}

static int ble_command(int argc, char **argv)
{
    static const char *usage = "ERR USAGE: ble scan [1..30s]|status|connect <MAC> public|random|disconnect|services|chars <start> <end>|read <handle>|write <handle> <hex max20bytes> confirm";
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        printf("OK BLE initialized=%d connected=%d handle=%u connecting=%d\n", initialized,
            atomic_load(&connection) != BLE_HS_CONN_HANDLE_NONE, atomic_load(&connection), atomic_load(&connecting));
        return ESP_OK;
    }
    if (argc < 2 || argc > 5) {
        puts(usage);
        return ESP_ERR_INVALID_ARG;
    }
    bool scan = strcmp(argv[1], "scan") == 0;
    bool central = strcmp(argv[1], "connect") == 0 || strcmp(argv[1], "disconnect") == 0 ||
        strcmp(argv[1], "services") == 0 || strcmp(argv[1], "chars") == 0 ||
        strcmp(argv[1], "read") == 0 || strcmp(argv[1], "write") == 0;
    if (!scan && !central) { puts(usage); return ESP_ERR_INVALID_ARG; }
    uint32_t seconds = BLE_DEFAULT_SCAN_SECONDS;
    if (scan) {
        if (argc > 3 || (argc == 3 && (!lab_arg_u32(argv[2], BLE_MAX_SCAN_SECONDS, &seconds) || !seconds))) {
            puts(usage);
            return ESP_ERR_INVALID_ARG;
        }
    }
    esp_err_t error = ble_start();
    if (error != ESP_OK) {
        printf("ERR ble start: %s\n", esp_err_to_name(error));
        return error;
    }
    int result = scan ? ble_scan((int)seconds) : central_command(argc, argv);
    if (result == ESP_ERR_INVALID_ARG) puts(usage);
    return result;
}

static esp_err_t diagnose(void)
{
    printf("SKIP ble: radio stays off until the first ble command\n");
    return ESP_OK;
}

const lab_module_t lab_ble_module = {
    .name = "ble",
    .description = "BLE: scan|status|connect|disconnect|services|chars|read|write confirm",
    .command = ble_command,
    .diagnose = diagnose,
    .implemented = true,
};
