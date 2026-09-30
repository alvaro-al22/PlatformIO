#include <cstdio>
#include <cstring>
#include "core/command_args.h"
#include "core/spi_bus.h"
#include "lab_config.h"
#include "RadioLib.h"
#include "hal/ESP-IDF/EspHal.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static EspHal hal(LAB_PIN_SPI_SCK, LAB_PIN_SPI_MISO, LAB_PIN_SPI_MOSI);
static Module radio_module(&hal, LAB_PIN_CC1101_CS, LAB_PIN_CC1101_GDO0, RADIOLIB_NC, LAB_PIN_CC1101_GDO2);
static CC1101 radio(&radio_module);
static bool ready;
static int64_t last_tx = -60000000;

extern "C" int lab_cc1101_command(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "status") == 0) {
        std::printf("OK CC1101 %s; packet FSK only; no rolling-code emulation\n", ready ? "initialized" : "not initialized");
        return ESP_OK;
    }
    if (argc == 3 && std::strcmp(argv[1], "init") == 0) {
        float frequency = std::strcmp(argv[2], "EU433") == 0 ? 433.92f :
            std::strcmp(argv[2], "EU868") == 0 ? 868.35f :
            std::strcmp(argv[2], "US915") == 0 ? 915.0f : 0;
        if (!frequency) return ESP_ERR_INVALID_ARG;
        esp_err_t error = lab_spi_start();
        if (error != ESP_OK) return error;
        ready = false;
        int16_t result = radio.begin(frequency, 4.8f, 5.0f, 58.0f, -10, 16);
        if (result == RADIOLIB_ERR_NONE) result = radio.standby();
        ready = result == RADIOLIB_ERR_NONE;
        std::printf("%s CC1101 init=%d frequency=%.2f MHz power=-10dBm; use matching antenna and local regulations\n",
            ready ? "OK" : "ERR", result, frequency);
        return ready ? ESP_OK : ESP_FAIL;
    }
    uint32_t seconds = 5;
    bool receive = (argc == 2 || argc == 3) && std::strcmp(argv[1], "rx") == 0;
    bool transmit = argc == 4 && std::strcmp(argv[1], "tx") == 0 && std::strcmp(argv[3], "confirm") == 0;
    bool sleep = argc == 2 && std::strcmp(argv[1], "sleep") == 0;
    uint8_t data[32];
    size_t length = 0;
    if ((!receive && !transmit && !sleep) || (receive && argc == 3 &&
        (!lab_arg_u32(argv[2], 10, &seconds) || !seconds)) ||
        (transmit && !lab_arg_hex(argv[2], data, sizeof(data), &length))) {
        std::puts("ERR USAGE: cc1101 init EU433|EU868|US915|status|rx [1..10 seconds]|tx <hex max32bytes> confirm|sleep");
        return ESP_ERR_INVALID_ARG;
    }
    if (!ready) { std::puts("ERR cc1101 init first; requires physical CC1101"); return ESP_ERR_INVALID_STATE; }
    if (sleep) { int16_t result = radio.sleep(); ready = false; return result == 0 ? ESP_OK : ESP_FAIL; }
    if (transmit) {
        int64_t now = esp_timer_get_time();
        if (now - last_tx < 60000000) { std::puts("ERR TX limit: one packet per 60 seconds"); return ESP_ERR_INVALID_STATE; }
        last_tx = now;
        int16_t result = radio.transmit(data, length);
        int16_t cleanup = radio.standby();
        if (cleanup != 0) ready = false;
        std::printf("%s TX result=%d cleanup=%d; receiver acknowledgement not available\n", result == 0 ? "OK" : "ERR", result, cleanup);
        return result == 0 && cleanup == 0 ? ESP_OK : ESP_FAIL;
    }
    int16_t result = radio.startReceive();
    int64_t deadline = esp_timer_get_time() + seconds * 1000000LL;
    while (result == 0 && !gpio_get_level((gpio_num_t)LAB_PIN_CC1101_GDO0) && esp_timer_get_time() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (result == 0 && !gpio_get_level((gpio_num_t)LAB_PIN_CC1101_GDO0)) result = RADIOLIB_ERR_RX_TIMEOUT;
    if (result == 0) {
        length = radio.getPacketLength();
        if (length > sizeof(data)) result = RADIOLIB_ERR_PACKET_TOO_LONG;
        else result = radio.readData(data, length);
        if (result == 0) {
            std::printf("OK RX RSSI=%.1f data=", radio.getRSSI());
            for (size_t index = 0; index < length; ++index) std::printf("%02x", data[index]);
            std::puts("");
        }
    }
    int16_t cleanup = radio.standby();
    if (cleanup != 0) ready = false;
    if (result != 0) std::printf("ERR RX result=%d\n", result);
    return result == 0 && cleanup == 0 ? ESP_OK : ESP_FAIL;
}