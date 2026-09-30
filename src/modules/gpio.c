#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/module.h"
#include "core/command_args.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const int user_pins[] = {7, 15, 16, 47};
static bool outputs[4];
static int pwm_pin = -1;

static int gpio_command(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "pins") == 0) {
        puts("OK user GPIO: 7 15 16 47; 3.3V only; all other pins reserved");
        return ESP_OK;
    }
    uint32_t pin;
    if (argc < 3 || !lab_arg_u32(argv[2], 48, &pin)) goto usage;
    size_t index = 0;
    while (index < 4 && user_pins[index] != (int)pin) ++index;
    if (index == 4) {
        puts("ERR reserved pin; allowed GPIO: 7 15 16 47");
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 3 && strcmp(argv[1], "release") == 0) {
        if (pwm_pin == (int)pin) {
            esp_err_t error = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            if (error != ESP_OK) return error;
            pwm_pin = -1;
        }
        outputs[index] = false;
        return gpio_reset_pin((gpio_num_t)pin);
    }
    if (argc == 4 && strcmp(argv[1], "mode") == 0) {
        bool output = strcmp(argv[3], "out") == 0;
        bool up = strcmp(argv[3], "pullup") == 0;
        bool down = strcmp(argv[3], "pulldown") == 0;
        if (!output && !up && !down && strcmp(argv[3], "in") != 0) goto usage;
        if (pwm_pin == (int)pin) {
            puts("ERR release PWM pin first");
            return ESP_ERR_INVALID_STATE;
        }
        if (output) {
            esp_err_t error = gpio_set_level((gpio_num_t)pin, 0);
            if (error != ESP_OK) return error;
        }
        gpio_config_t config = {
            .pin_bit_mask = 1ULL << pin,
            .mode = output ? GPIO_MODE_INPUT_OUTPUT : GPIO_MODE_INPUT,
            .pull_up_en = up,
            .pull_down_en = down,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t error = gpio_config(&config);
        if (error == ESP_OK) outputs[index] = output;
        return error;
    }
    if (argc == 3 && strcmp(argv[1], "read") == 0) {
        if (pwm_pin == (int)pin) {
            puts("ERR release PWM pin before reading; PWM input level is not measured");
            return ESP_ERR_INVALID_STATE;
        }
        if (!outputs[index]) {
            esp_err_t error = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            if (error != ESP_OK) return error;
        }
        printf("OK GPIO%u=%d\n", (unsigned)pin, gpio_get_level((gpio_num_t)pin));
        return ESP_OK;
    }
    if (argc == 4 && strcmp(argv[1], "write") == 0) {
        uint32_t level;
        if (!lab_arg_u32(argv[3], 1, &level)) goto usage;
        if (!outputs[index] || pwm_pin == (int)pin) {
            puts("ERR configure gpio mode <pin> out first");
            return ESP_ERR_INVALID_STATE;
        }
        return gpio_set_level((gpio_num_t)pin, level);
    }
    if (argc == 5 && strcmp(argv[1], "pwm") == 0) {
        uint32_t frequency, duty;
        if (!lab_arg_u32(argv[3], 40000, &frequency) || frequency == 0 ||
            !lab_arg_u32(argv[4], 100, &duty)) goto usage;
        if (pwm_pin >= 0 && pwm_pin != (int)pin) {
            puts("ERR only one PWM output; release the previous pin first");
            return ESP_ERR_INVALID_STATE;
        }
        ledc_timer_config_t timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
            .duty_resolution = LEDC_TIMER_10_BIT, .freq_hz = frequency,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        esp_err_t error = ledc_timer_config(&timer);
        if (error != ESP_OK) return error;
        ledc_channel_config_t channel = {
            .gpio_num = (int)pin, .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0,
            .duty = duty * 1023 / 100,
        };
        error = ledc_channel_config(&channel);
        if (error == ESP_OK) {
            pwm_pin = (int)pin;
            outputs[index] = true;
        }
        return error;
    }
usage:
    puts("ERR USAGE: gpio pins|mode <pin> in|out|pullup|pulldown|read <pin>|write <pin> 0|1|pwm <pin> <Hz 1..40000> <percent>|release <pin>");
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t diagnose(void)
{
    puts("SKIP gpio: pin policy implemented; electrical behavior not tested");
    return ESP_OK;
}

const lab_module_t lab_gpio_module = {
    .name = "gpio",
    .description = "GPIO: pins|mode|read|write|pwm|release (7,15,16,47)",
    .command = gpio_command,
    .diagnose = diagnose,
    .implemented = true,
};