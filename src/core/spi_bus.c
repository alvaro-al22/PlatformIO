#include "core/spi_bus.h"
#include "lab_config.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

esp_err_t lab_spi_start(void)
{
    static bool ready;
    if (ready) return ESP_OK;
    esp_err_t error = gpio_set_level(LAB_PIN_CC1101_CS, 1);
    if (error == ESP_OK) error = gpio_set_level(LAB_PIN_SD_CS, 1);
    gpio_config_t pins = {.pin_bit_mask = (1ULL << LAB_PIN_CC1101_CS) | (1ULL << LAB_PIN_SD_CS),
        .mode = GPIO_MODE_OUTPUT};
    if (error == ESP_OK) error = gpio_config(&pins);
    if (error != ESP_OK) return error;
    spi_bus_config_t bus = {.mosi_io_num = LAB_PIN_SPI_MOSI, .miso_io_num = LAB_PIN_SPI_MISO,
        .sclk_io_num = LAB_PIN_SPI_SCK, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .data4_io_num = -1, .data5_io_num = -1, .data6_io_num = -1, .data7_io_num = -1,
        .max_transfer_sz = 4096};
    error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    ready = error == ESP_OK;
    return error;
}