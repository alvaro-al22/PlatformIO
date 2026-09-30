#include <stdio.h>
#include <string.h>
#include "core/module.h"
#include "core/command_args.h"
#include "lab_config.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs.h"

#define IR_SYMBOL_LIMIT 256
static struct {
    uint32_t version;
    uint32_t carrier;
    uint32_t count;
    rmt_symbol_word_t symbols[IR_SYMBOL_LIMIT];
} signal = {.version = 1, .carrier = 38000};
static rmt_channel_handle_t receiver, transmitter;
static rmt_encoder_handle_t encoder;
static QueueHandle_t received;
static bool fault;

static bool on_receive(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *event, void *context)
{
    (void)channel;
    BaseType_t woken = pdFALSE;
    size_t count = event->num_symbols;
    xQueueSendFromISR((QueueHandle_t)context, &count, &woken);
    return woken == pdTRUE;
}

static esp_err_t learn(uint32_t seconds)
{
    signal.count = 0;
    if (!receiver) {
        if (!received) received = xQueueCreate(1, sizeof(size_t));
        if (!received) return ESP_ERR_NO_MEM;
        rmt_rx_channel_config_t config = {.gpio_num = LAB_PIN_IR_RX,
            .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 1000000,
            .mem_block_symbols = 64, .flags.invert_in = true};
        esp_err_t error = rmt_new_rx_channel(&config, &receiver);
        if (error != ESP_OK) return error;
        rmt_rx_event_callbacks_t callbacks = {.on_recv_done = on_receive};
        error = rmt_rx_register_event_callbacks(receiver, &callbacks, received);
        if (error != ESP_OK) { fault = true; return error; }
    }
    xQueueReset(received);
    esp_err_t error = rmt_enable(receiver);
    if (error != ESP_OK) return error;
    rmt_receive_config_t config = {.signal_range_min_ns = 100000, .signal_range_max_ns = 15000000};
    error = rmt_receive(receiver, signal.symbols, sizeof(signal.symbols), &config);
    size_t count = 0;
    if (error == ESP_OK && xQueueReceive(received, &count, pdMS_TO_TICKS(seconds * 1000)) != pdTRUE)
        error = ESP_ERR_TIMEOUT;
    esp_err_t cleanup = rmt_disable(receiver);
    if (cleanup != ESP_OK) { fault = true; return cleanup; }
    if (error != ESP_OK) return error;
    if (!count || count >= IR_SYMBOL_LIMIT) return ESP_ERR_INVALID_SIZE;
    signal.count = count;
    printf("OK IR learned %u symbols; carrier assumed %u Hz, not measured\n",
        (unsigned)signal.count, (unsigned)signal.carrier);
    return ESP_OK;
}

static esp_err_t send_signal(void)
{
    if (!signal.count) return ESP_ERR_INVALID_STATE;
    if (!transmitter) {
        rmt_tx_channel_config_t config = {.gpio_num = LAB_PIN_IR_TX,
            .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 1000000,
            .mem_block_symbols = 64, .trans_queue_depth = 1};
        esp_err_t error = rmt_new_tx_channel(&config, &transmitter);
        if (error != ESP_OK) return error;
    }
    if (!encoder) {
        rmt_copy_encoder_config_t config = {};
        esp_err_t error = rmt_new_copy_encoder(&config, &encoder);
        if (error != ESP_OK) return error;
    }
    rmt_carrier_config_t carrier = {.frequency_hz = signal.carrier, .duty_cycle = 0.33f};
    esp_err_t error = rmt_apply_carrier(transmitter, &carrier);
    if (error != ESP_OK) return error;
    error = rmt_enable(transmitter);
    if (error != ESP_OK) return error;
    rmt_transmit_config_t config = {.loop_count = 0, .flags.eot_level = 0};
    error = rmt_transmit(transmitter, encoder, signal.symbols, signal.count * sizeof(signal.symbols[0]), &config);
    if (error == ESP_OK) error = rmt_tx_wait_all_done(transmitter, 10000);
    esp_err_t cleanup = rmt_disable(transmitter);
    if (cleanup != ESP_OK) { fault = true; return cleanup; }
    return error;
}

static int command(int argc, char **argv)
{
    if (fault) { puts("ERR IR driver state uncertain; reset device"); return ESP_ERR_INVALID_STATE; }
    uint32_t value;
    if (argc == 3 && strcmp(argv[1], "carrier") == 0 && lab_arg_u32(argv[2], 60, &value) && value >= 30) {
        signal.carrier = value * 1000;
        return ESP_OK;
    }
    if ((argc == 2 || argc == 3) && strcmp(argv[1], "learn") == 0) {
        value = 5;
        if (argc == 3 && (!lab_arg_u32(argv[2], 30, &value) || !value)) return ESP_ERR_INVALID_ARG;
        return learn(value);
    }
    if (argc == 3 && strcmp(argv[1], "send") == 0 && strcmp(argv[2], "confirm") == 0) return send_signal();
    if (argc == 2 && strcmp(argv[1], "show") == 0) {
        printf("OK IR count=%u carrier=%uHz\n", (unsigned)signal.count, (unsigned)signal.carrier);
        for (uint32_t index = 0; index < signal.count; ++index) {
            printf("%u:%u %u:%u\n", signal.symbols[index].level0, signal.symbols[index].duration0,
                signal.symbols[index].level1, signal.symbols[index].duration1);
        }
        return ESP_OK;
    }
    bool load = argc == 3 && strcmp(argv[1], "load") == 0 && lab_arg_name(argv[2]);
    bool save = argc == 4 && strcmp(argv[1], "save") == 0 && lab_arg_name(argv[2]) && strcmp(argv[3], "confirm") == 0;
    bool remove = argc == 4 && strcmp(argv[1], "remove") == 0 && lab_arg_name(argv[2]) && strcmp(argv[3], "confirm") == 0;
    bool list = argc == 2 && strcmp(argv[1], "list") == 0;
    if (!load && !save && !remove && !list) {
        puts("ERR USAGE: ir learn [1..30s]|show|carrier <30..60kHz>|send confirm|list|load <name>|save <name> confirm|remove <name> confirm");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = lab_nvs_init();
    if (error != ESP_OK) return error;
    if (list) {
        nvs_iterator_t iterator = NULL;
        error = nvs_entry_find("nvs", "ir_signals", NVS_TYPE_BLOB, &iterator);
        while (error == ESP_OK) {
            nvs_entry_info_t info;
            error = nvs_entry_info(iterator, &info);
            if (error != ESP_OK) break;
            puts(info.key);
            error = nvs_entry_next(&iterator);
        }
        nvs_release_iterator(iterator);
        return error == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : error;
    }
    nvs_handle_t handle;
    error = nvs_open("ir_signals", load ? NVS_READONLY : NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    if (load) {
        size_t length = sizeof(signal);
        signal.count = 0;
        error = nvs_get_blob(handle, argv[2], &signal, &length);
        if (error == ESP_OK && (length != sizeof(signal) || signal.version != 1 ||
            signal.carrier < 30000 || signal.carrier > 60000 || signal.count == 0 || signal.count >= IR_SYMBOL_LIMIT)) error = ESP_ERR_INVALID_SIZE;
        if (error != ESP_OK) { memset(&signal, 0, sizeof(signal)); signal.version = 1; signal.carrier = 38000; }
    } else {
        error = remove ? nvs_erase_key(handle, argv[2]) : signal.count ?
            nvs_set_blob(handle, argv[2], &signal, sizeof(signal)) : ESP_ERR_INVALID_STATE;
        if (error == ESP_OK) error = nvs_commit(handle);
    }
    nvs_close(handle);
    return error;
}

static esp_err_t diagnose(void)
{
    puts("SKIP ir: RMT raw learn/send available; receiver/emitter not probed");
    return ESP_OK;
}

const lab_module_t lab_ir_module = {
    .name = "ir", .description = "IR: learn|show|carrier|send confirm|list|save|load|remove",
    .command = command, .diagnose = diagnose, .implemented = true,
};