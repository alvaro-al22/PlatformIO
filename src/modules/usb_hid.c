#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "core/module.h"
#include "core/command_args.h"
#include "tinyusb.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool started;
static atomic_bool key_pending;
static const uint8_t reports[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2))
};
static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bMaxPacketSize0 = 64, .idVendor = 0xcafe,
    .idProduct = 0x4011, .bcdDevice = 0x0100, .iManufacturer = 1,
    .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
static const uint8_t configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(reports), 0x81, 16, 10)
};
static const char language[] = {0x09, 0x04};
static const char *strings[] = {language, "USB Lab", "USB Lab keyboard/mouse", "LAB0001"};
static const uint8_t ascii_keys[128][2] = {HID_ASCII_TO_KEYCODE};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return reports;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t *buffer, uint16_t length)
{
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)length;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t const *buffer, uint16_t length)
{
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)length;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t length)
{
    (void)instance;
    if (length < 2 || report[0] != 1) return;
    bool pressed = false;
    for (uint16_t index = 1; index < length; ++index) pressed |= report[index] != 0;
    if (pressed) tud_hid_keyboard_report(1, 0, NULL);
    else atomic_store(&key_pending, false);
}

static esp_err_t key(uint8_t usage, uint8_t modifier)
{
    if (atomic_load(&key_pending)) return ESP_ERR_INVALID_STATE;
    int64_t deadline = esp_timer_get_time() + 1000000;
    while (tud_mounted() && !tud_hid_ready() && esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(1));
    if (!tud_mounted() || !tud_hid_ready()) return ESP_ERR_TIMEOUT;
    uint8_t keys[6] = {usage};
    atomic_store(&key_pending, true);
    if (!tud_hid_keyboard_report(1, modifier, keys)) {
        atomic_store(&key_pending, false);
        return ESP_FAIL;
    }
    while (tud_mounted() && atomic_load(&key_pending) && esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(1));
    return tud_mounted() && !atomic_load(&key_pending) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static bool movement(const char *text, int8_t *result)
{
    bool negative = text[0] == '-';
    uint32_t value;
    if (!lab_arg_u32(text + negative, 127, &value)) return false;
    *result = negative ? -(int)value : (int)value;
    return true;
}

static int command(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        printf("OK USB %s mounted=%d pending_key=%d\n", started ? "started" : "off",
            started && tud_mounted(), atomic_load(&key_pending));
        return ESP_OK;
    }
    if (argc == 3 && strcmp(argv[2], "confirm") == 0 && strcmp(argv[1], "start") == 0) {
        if (started) return ESP_OK;
        tinyusb_config_t config = {.device_descriptor = &device, .string_descriptor = strings,
            .string_descriptor_count = 4, .configuration_descriptor = configuration};
        esp_err_t error = tinyusb_driver_install(&config);
        started = error == ESP_OK;
        if (started) puts("OK HID started on native USB; USB-UART remains the console; US keyboard mapping");
        return error;
    }
    if (argc == 3 && strcmp(argv[2], "confirm") == 0 && strcmp(argv[1], "stop") == 0) {
        if (!started) return ESP_OK;
        esp_err_t error = tinyusb_driver_uninstall();
        if (error == ESP_OK) { started = false; atomic_store(&key_pending, false); }
        return error;
    }
    bool text = argc == 4 && strcmp(argv[1], "text") == 0 && strcmp(argv[3], "confirm") == 0;
    bool press = argc == 4 && strcmp(argv[1], "key") == 0 && strcmp(argv[3], "confirm") == 0;
    bool mouse = argc == 5 && strcmp(argv[1], "mouse") == 0 && strcmp(argv[4], "confirm") == 0;
    uint32_t usage = 0;
    int8_t horizontal = 0, vertical = 0;
    if ((!text && !press && !mouse) || (press && (!lab_arg_u32(argv[2], 115, &usage) || usage < 4)) ||
        (mouse && (!movement(argv[2], &horizontal) || !movement(argv[3], &vertical)))) {
        puts("ERR USAGE: usb status|start confirm|stop confirm|text <ASCII> confirm|key <HID usage 4..115> confirm|mouse <dx> <dy> confirm");
        return ESP_ERR_INVALID_ARG;
    }
    if (text) {
        for (const unsigned char *cursor = (const unsigned char *)argv[2]; *cursor; ++cursor)
            if (*cursor < 32 || *cursor >= 127 || !ascii_keys[*cursor][1]) return ESP_ERR_INVALID_ARG;
    }
    if (!started || !tud_mounted() || atomic_load(&key_pending)) {
        puts("ERR USB not ready or key release pending; connect native USB, or stop/restart HID");
        return ESP_ERR_INVALID_STATE;
    }
    if (mouse) return tud_hid_ready() && tud_hid_mouse_report(2, 0, horizontal, vertical, 0, 0) ? ESP_OK : ESP_FAIL;
    if (press) return key((uint8_t)usage, 0);
    for (const unsigned char *cursor = (const unsigned char *)argv[2]; *cursor; ++cursor) {
        esp_err_t error = key(ascii_keys[*cursor][1], ascii_keys[*cursor][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

static esp_err_t diagnose(void)
{
    puts("SKIP usb: HID inactive until usb start confirm; host behavior not tested");
    return ESP_OK;
}

const lab_module_t lab_usb_module = {
    .name = "usb", .description = "Native USB HID: status|start|stop|text|key|mouse (explicit confirm)",
    .command = command, .diagnose = diagnose, .implemented = true,
};