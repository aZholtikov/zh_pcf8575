# ESP32 ESP-IDF component for PCF8575 16-bit I/O expander

## Tested on

1. [ESP32 ESP-IDF v6.0.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/index.html)

## SAST Tools

[PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Features

1. Support of 8 expanders on one bus.
2. Support of output and input GPIO's work mode.
3. Support of interrupts from input GPIO's.

## Note

1. Enable interrupt support only if input GPIO's are used.
2. All the INT GPIO's on the extenders must be connected to the one GPIO on ESP.
3. The input GPIO's are always pullup to the power supply.

## Attention

For correct operation, please enable the following settings in the menuconfig:

```text
GPIO_CTRL_FUNC_IN_IRAM
I2C_ISR_IRAM_SAFE
I2C_MASTER_ISR_HANDLER_IN_IRAM
```

## Dependencies

1. [zh_vector](https://github.com/aZholtikov/zh_vector)

## Using

In an existing project, run the following command to install the components:

```text
cd ../your_project/components
git clone https://github.com/aZholtikov/zh_pcf8575
git clone https://github.com/aZholtikov/zh_vector
```

In the application, add the component:

```c
#include "zh_pcf8575.h"
```

## Examples

One expander on bus. All GPIO's as output (except P00 and P16 - input). Interrupt is enable:

```c
#include "zh_pcf8575.h"

#define I2C_PORT (I2C_NUM_MAX - 1)

zh_pcf8575_handle_t pcf8575_handle = {0};

void zh_pcf8575_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void print_gpio_status(const char *message, uint16_t reg)
{
    printf("%s", message);
    for (uint8_t i = 0; i <= 15; ++i)
    {
        printf("%c", (reg & 0x8000) ? '1' : '0');
        reg <<= 1;
    }
    printf(".\n");
}

void app_main(void)
{
    esp_log_level_set("zh_pcf8575", ESP_LOG_ERROR);
    esp_log_level_set("zh_vector", ESP_LOG_ERROR);
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = GPIO_NUM_22,
        .sda_io_num = GPIO_NUM_21,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    esp_event_loop_create_default();
    esp_event_handler_instance_register(ZH_PCF8575, ESP_EVENT_ANY_ID, &zh_pcf8575_event_handler, NULL, NULL);
    zh_pcf8575_init_config_t config = ZH_PCF8575_INIT_CONFIG_DEFAULT();
    config.i2c_handle = i2c_bus_handle;
    config.i2c_address = 0x20;
    config.p00_gpio_work_mode = ZH_PCF8575_GPIO_INPUT;
    config.p16_gpio_work_mode = ZH_PCF8575_GPIO_INPUT;
    config.interrupt_gpio = GPIO_NUM_14;
    zh_pcf8575_init(&config, &pcf8575_handle);
    uint16_t reg = 0;
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("GPIO status: ", reg);
    printf("Set P7 to 1, P1 to 1 and P0 to 0.\n");
    zh_pcf8575_write(&pcf8575_handle, 0b0000000010000010);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("GPIO status: ", reg);
    printf("Sets P0 to 0.\n");
    zh_pcf8575_write_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P00, ZH_PCF8575_GPIO_LOW);
    bool gpio = 0;
    zh_pcf8575_read_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P00, &gpio);
    printf("P0 status: %d.\n", gpio);
    printf("Set P1 to 0.\n");
    zh_pcf8575_write_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P01, ZH_PCF8575_GPIO_LOW);
    zh_pcf8575_read_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P01, &gpio);
    printf("P1 status: %d.\n", gpio);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("GPIO status: ", reg);
    printf("Reset all GPIO.\n");
    zh_pcf8575_reset(&pcf8575_handle);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("GPIO status: ", reg);
    for (;;)
    {
        const zh_pcf8575_stats_t *stats = zh_pcf8575_get_stats();
        printf("Number of i2c driver error: %ld.\n", stats->i2c_driver_error);
        printf("Number of event post error: %ld.\n", stats->event_post_error);
        printf("Number of vector error: %ld.\n", stats->vector_error);
        printf("Number of queue overflow error: %ld.\n", stats->queue_overflow_error);
        printf("Minimum free stack size: %ld.\n", stats->min_stack_size);
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void zh_pcf8575_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    zh_pcf8575_event_on_isr_t *event = event_data;
    printf("Interrupt happened on device address 0x%02X on GPIO number %d at level %d at time %lld.\n", event->i2c_address, event->gpio_number,
           event->gpio_level, event->interrupt_time);
}
```
