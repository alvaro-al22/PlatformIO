#include <stdio.h>
#include <string.h>
#include "core/module.h"
#include "core/command_args.h"
#include "lab_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static int command(int argc, char **argv)
{
    uint32_t seconds = 5;
    if ((argc != 2 && argc != 3) || strcmp(argv[1], "read") != 0 ||
        (argc == 3 && (!lab_arg_u32(argv[2], 30, &seconds) || !seconds))) {
        puts("ERR USAGE: rfid read [seconds 1..30] (RDM6300 UART; RX GPIO2 at 3.3V)");
        return ESP_ERR_INVALID_ARG;
    }
    uart_config_t config = {.baud_rate = 9600, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    esp_err_t error = uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
    if (error != ESP_OK) return error;
    error = uart_param_config(UART_NUM_1, &config);
    if (error == ESP_OK) error = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE,
        LAB_PIN_RFID_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    char frame[13] = {0};
    size_t used = 0;
    bool collecting = false, found = false;
    int64_t deadline = esp_timer_get_time() + seconds * 1000000LL;
    while (error == ESP_OK && !found && esp_timer_get_time() < deadline) {
        uint8_t byte;
        int received = uart_read_bytes(UART_NUM_1, &byte, 1, pdMS_TO_TICKS(20));
        if (received < 0) { error = ESP_FAIL; break; }
        if (!received) continue;
        if (byte == 2) { used = 0; collecting = true; continue; }
        if (!collecting) continue;
        if (byte == 3 && used == 12) {
            frame[12] = 0;
            uint8_t data[6];
            size_t length = 0;
            if (lab_arg_hex(frame, data, sizeof(data), &length) && length == 6 &&
                (data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4]) == data[5]) {
                printf("OK EM4100/RDM6300 ID=%02x%02x%02x%02x%02x\n",
                    data[0], data[1], data[2], data[3], data[4]);
                found = true;
            }
            collecting = false;
        } else if (used < 12 && lab_hex_digit((char)byte) >= 0) {
            frame[used++] = (char)byte;
        } else collecting = false;
    }
    esp_err_t cleanup = uart_driver_delete(UART_NUM_1);
    gpio_reset_pin(LAB_PIN_RFID_RX);
    if (cleanup != ESP_OK) return cleanup;
    if (error != ESP_OK) return error;
    if (!found) puts("SKIP no valid tag frame; reader absent, tag absent or unsupported");
    return found ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t diagnose(void)
{
    puts("SKIP rfid: RDM6300 reader not probed; read-only EM4100 IDs");
    return ESP_OK;
}

const lab_module_t lab_rfid_module = {
    .name = "rfid", .description = "125 kHz: read [seconds] (RDM6300/EM4100 only)",
    .command = command, .diagnose = diagnose, .implemented = true,
};