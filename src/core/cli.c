#include "core/cli.h"
#include "core/module.h"
#include "lab_config.h"
#include "esp_console.h"

esp_err_t lab_cli_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    config.prompt = LAB_PROMPT;
    config.max_cmdline_length = LAB_LINE_LENGTH;
    config.max_history_len = LAB_HISTORY_LENGTH;
    config.task_stack_size = LAB_CONSOLE_STACK_SIZE;
    esp_console_dev_uart_config_t uart = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    uart.baud_rate = LAB_BAUD_RATE;
    esp_err_t error = esp_console_new_repl_uart(&uart, &config, &repl);
    if (error != ESP_OK) {
        return error;
    }

    const esp_console_cmd_t menu = {
        .command = "menu",
        .help = "List available modules and implementation status",
        .func = lab_menu,
    };
    error = esp_console_cmd_register(&menu);
    size_t count = 0;
    const lab_module_t *const *modules = lab_modules(&count);
    for (size_t index = 0; error == ESP_OK && index < count; ++index) {
        const esp_console_cmd_t command = {
            .command = modules[index]->name,
            .help = modules[index]->description,
            .func = modules[index]->command,
        };
        error = esp_console_cmd_register(&command);
    }
    if (error == ESP_OK) {
        error = esp_console_start_repl(repl);
    }
    if (error != ESP_OK) {
        repl->del(repl);
    }
    return error;
}