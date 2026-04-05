#include "zh_pcf8575.h"

static const char *TAG = "zh_pcf8575";

#define ZH_LOGI(msg, ...) ESP_LOGI(TAG, msg, ##__VA_ARGS__)
#define ZH_LOGE(msg, err, ...) ESP_LOGE(TAG, "[%s:%d:%s] " msg, __FILE__, __LINE__, esp_err_to_name(err), ##__VA_ARGS__)

#define ZH_ERROR_CHECK(cond, err, cleanup, msg, ...) \
    if (!(cond))                                     \
    {                                                \
        ZH_LOGE(msg, err, ##__VA_ARGS__);            \
        cleanup;                                     \
        return err;                                  \
    }

TaskHandle_t zh_pcf8575 = NULL;
static SemaphoreHandle_t _interrupt_semaphore = NULL;

volatile static gpio_num_t _interrupt_gpio = GPIO_NUM_MAX;
volatile static uint8_t _i2c_matrix[8] = {0};
static const uint16_t _gpio_matrix[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
static zh_pcf8575_stats_t _stats = {0};

static zh_vector_t _vector = {0};

static esp_err_t _zh_pcf8575_validate_config(const zh_pcf8575_init_config_t *config);
static esp_err_t _zh_pcf8575_gpio_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle);
static esp_err_t _zh_pcf8575_i2c_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle);
static esp_err_t _zh_pcf8575_resources_init(const zh_pcf8575_init_config_t *config);
static esp_err_t _zh_pcf8575_task_init(const zh_pcf8575_init_config_t *config);
static void _zh_pcf8575_isr_handler(void *arg);
static void _zh_pcf8575_isr_processing_task(void *pvParameter);
static esp_err_t _zh_pcf8575_read_register(zh_pcf8575_handle_t *handle, uint16_t *reg);
static esp_err_t _zh_pcf8575_write_register(zh_pcf8575_handle_t *handle, uint16_t reg);

ESP_EVENT_DEFINE_BASE(ZH_PCF8575);

esp_err_t zh_pcf8575_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle) // -V2008
{
    ZH_LOGI("PCF8575 initialization started.");
    ZH_ERROR_CHECK(config != NULL && handle != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 initialization failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == false, ESP_ERR_INVALID_STATE, NULL, "PCF8575 initialization failed. PCF8575 is already initialized.");
    ZH_ERROR_CHECK(_zh_pcf8575_validate_config(config) == ESP_OK, ESP_FAIL, NULL, "PCF8575 initialization failed. Initial configuration check failed.");
    ZH_ERROR_CHECK(_zh_pcf8575_i2c_init(config, handle) == ESP_OK, ESP_FAIL, NULL, "PCF8575 initialization failed. Failed to add I2C device.");
    ZH_ERROR_CHECK(_zh_pcf8575_write_register(handle, handle->gpio_work_mode) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(i2c_master_bus_rm_device(handle->dev_handle) == ESP_OK, ESP_FAIL, NULL, "I2C remove device failed."), "PCF8575 initialization failed. Failed extender initial GPIO setup.");
    if (config->interrupt_gpio < GPIO_NUM_MAX && handle->gpio_work_mode != 0)
    {
        ZH_ERROR_CHECK(_zh_pcf8575_gpio_init(config, handle) == ESP_OK, ESP_FAIL,
                       ZH_ERROR_CHECK(i2c_master_bus_rm_device(handle->dev_handle) == ESP_OK, ESP_FAIL, NULL, "I2C remove device failed."), "PCF8575 initialization failed. Interrupt GPIO initialization failed.");
        ZH_ERROR_CHECK(_zh_pcf8575_resources_init(config) == ESP_OK, ESP_FAIL,
                       ZH_ERROR_CHECK(i2c_master_bus_rm_device(handle->dev_handle) == ESP_OK, ESP_FAIL, NULL, "I2C remove device failed.");
                       ZH_ERROR_CHECK(gpio_isr_handler_remove(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Remove GPIO isr handler failed.");
                       ZH_ERROR_CHECK(gpio_reset_pin(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Reset GPIO failed.");
                       ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed."), "PCF8575 initialization failed. Resources initialization failed.");
        ZH_ERROR_CHECK(_zh_pcf8575_task_init(config) == ESP_OK, ESP_FAIL,
                       ZH_ERROR_CHECK(i2c_master_bus_rm_device(handle->dev_handle) == ESP_OK, ESP_FAIL, NULL, "I2C remove device failed.");
                       ZH_ERROR_CHECK(gpio_isr_handler_remove(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Remove gpio isr handler failed.");
                       ZH_ERROR_CHECK(gpio_reset_pin(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Reset gpio failed.");
                       ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed.");
                       vSemaphoreDelete(_interrupt_semaphore), "PCF8575 initialization failed. Task initialization failed.");
    }
    if (_stats.min_stack_size == 0)
    {
        _stats.min_stack_size = config->stack_size;
    }
    handle->is_initialized = true;
    for (uint8_t i = 0; i < sizeof(_i2c_matrix); ++i)
    {
        if (_i2c_matrix[i] == 0)
        {
            _i2c_matrix[i] = handle->i2c_address;
            break;
        }
    }
    ZH_LOGI("PCF8575 initialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_deinit(zh_pcf8575_handle_t *handle) // -V2008
{
    ZH_LOGI("PCF8575 deinitialization started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 deinitialization failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_INVALID_STATE, NULL, "PCF8575 deinitialization failed. PCF8575 not initialized.");
    if (_interrupt_gpio < GPIO_NUM_MAX && handle->gpio_work_mode != 0)
    {
        int16_t vector_size = (int16_t)zh_vector_get_size(&_vector);
        ZH_ERROR_CHECK(vector_size != ESP_FAIL, ESP_ERR_INVALID_STATE, NULL, "PCF8575 deinitialization failed. Vector get size fail.");
        for (uint16_t i = 0; i < vector_size; ++i)
        {
            zh_pcf8575_handle_t *temp_handle = zh_vector_get_item(&_vector, i);
            ZH_ERROR_CHECK(temp_handle != NULL, ESP_ERR_INVALID_STATE, NULL, "PCF8575 deinitialization failed. Vector get item fail.");
            if (handle->i2c_address == temp_handle->i2c_address)
            {
                ZH_ERROR_CHECK(zh_vector_delete_item(&_vector, i) == ESP_OK, ESP_FAIL, NULL, "PCF8575 deinitialization failed. Vector delete item fail.");
                break;
            }
        }
        vector_size = (int16_t)zh_vector_get_size(&_vector);
        ZH_ERROR_CHECK(vector_size != ESP_FAIL, ESP_ERR_INVALID_STATE, NULL, "PCF8575 deinitialization failed. Vector get size fail.");
        if (vector_size == 0)
        {
            ZH_ERROR_CHECK(gpio_isr_handler_remove(_interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "PCF8575 deinitialization failed. Remove GPIO isr handler failed.");
            ZH_ERROR_CHECK(gpio_reset_pin(_interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "PCF8575 deinitialization failed. Reset GPIO failed.");
            ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "PCF8575 deinitialization failed. Free vector failed.");
            vSemaphoreDelete(_interrupt_semaphore);
            vTaskDelete(zh_pcf8575);
            _interrupt_gpio = GPIO_NUM_MAX;
        }
    }
    if (handle->system != NULL)
    {
        heap_caps_free(handle->system);
    }
    ZH_ERROR_CHECK(i2c_master_bus_rm_device(handle->dev_handle) == ESP_OK, ESP_FAIL, NULL, "PCF8575 deinitialization failed. I2C remove device failed.");
    handle->is_initialized = false;
    for (uint8_t i = 0; i < sizeof(_i2c_matrix); ++i)
    {
        if (_i2c_matrix[i] == handle->i2c_address)
        {
            _i2c_matrix[i] = 0;
            break;
        }
    }
    ZH_LOGI("PCF8575 deinitialization completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_read(zh_pcf8575_handle_t *handle, uint16_t *reg)
{
    ZH_LOGI("PCF8575 read register started.");
    ZH_ERROR_CHECK(handle != NULL && reg != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 read register failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_NOT_FOUND, NULL, "PCF8575 read register failed. PCF8575 not initialized.");
    ZH_ERROR_CHECK(_zh_pcf8575_read_register(handle, reg) == ESP_OK, ESP_FAIL, NULL, "PCF8575 read register failed.");
    ZH_LOGI("PCF8575 read register completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_write(zh_pcf8575_handle_t *handle, uint16_t reg)
{
    ZH_LOGI("PCF8575 write register started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 write register failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_NOT_FOUND, NULL, "PCF8575 write register failed. PCF8575 not initialized.");
    ZH_ERROR_CHECK(_zh_pcf8575_write_register(handle, (reg | handle->gpio_work_mode)) == ESP_OK, ESP_FAIL, NULL, "PCF8575 write register failed.");
    ZH_LOGI("PCF8575 write register completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_reset(zh_pcf8575_handle_t *handle)
{
    ZH_LOGI("PCF8575 reset register started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 reset register failed. Invalid argument.");
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_NOT_FOUND, NULL, "PCF8575 reset register failed. PCF8575 not initialized.");
    ZH_ERROR_CHECK(_zh_pcf8575_write_register(handle, handle->gpio_work_mode) == ESP_OK, ESP_FAIL, NULL, "PCF8575 reset register failed.");
    ZH_LOGI("PCF8575 reset register completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_read_gpio(zh_pcf8575_handle_t *handle, zh_pcf8575_gpio_num_t gpio, bool *status) // -V2008
{
    ZH_LOGI("PCF8575 read GPIO started.");
    ZH_ERROR_CHECK(handle != NULL && status != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 read GPIO failed. Invalid argument.");
    ZH_ERROR_CHECK(gpio >= ZH_PCF8575_GPIO_NUM_P00 && gpio < ZH_PCF8575_GPIO_NUM_MAX, ESP_FAIL, NULL, "PCF8575 read GPIO failed. Invalid GPIO number.")
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_NOT_FOUND, NULL, "PCF8575 read GPIO failed. PCF8575 not initialized.");
    uint16_t gpio_temp = _gpio_matrix[gpio];
    uint16_t reg_temp = 0;
    ZH_ERROR_CHECK(_zh_pcf8575_read_register(handle, &reg_temp) == ESP_OK, ESP_FAIL, NULL, "PCF8575 read GPIO failed.");
    *status = ((reg_temp & gpio_temp) ? 1 : 0);
    ZH_LOGI("PCF8575 read GPIO completed successfully.");
    return ESP_OK;
}

esp_err_t zh_pcf8575_write_gpio(zh_pcf8575_handle_t *handle, zh_pcf8575_gpio_num_t gpio, bool status) // -V2008
{
    ZH_LOGI("PCF8575 write GPIO started.");
    ZH_ERROR_CHECK(handle != NULL, ESP_ERR_INVALID_ARG, NULL, "PCF8575 write GPIO failed. Invalid argument.");
    ZH_ERROR_CHECK(gpio >= ZH_PCF8575_GPIO_NUM_P00 && gpio < ZH_PCF8575_GPIO_NUM_MAX, ESP_FAIL, NULL, "PCF8575 write GPIO failed. Invalid GPIO number.")
    ZH_ERROR_CHECK(handle->is_initialized == true, ESP_ERR_NOT_FOUND, NULL, "PCF8575 write GPIO failed. PCF8575 not initialized.");
    uint16_t gpio_temp = _gpio_matrix[gpio];
    if (status == true)
    {
        ZH_ERROR_CHECK(_zh_pcf8575_write_register(handle, handle->gpio_status | handle->gpio_work_mode | gpio_temp) == ESP_OK, ESP_FAIL, NULL, "PCF8575 write GPIO failed.");
    }
    else
    {
        ZH_ERROR_CHECK(_zh_pcf8575_write_register(handle, (handle->gpio_status ^ gpio_temp) | handle->gpio_work_mode) == ESP_OK, ESP_FAIL, NULL, "PCF8575 write GPIO failed.");
    }
    ZH_LOGI("PCF8575 write GPIO completed successfully.");
    return ESP_OK;
}

const zh_pcf8575_stats_t *zh_pcf8575_get_stats(void)
{
    return &_stats;
}

void zh_pcf8575_reset_stats(void)
{
    ZH_LOGI("Error statistic reset started.");
    _stats.i2c_driver_error = 0;
    _stats.event_post_error = 0;
    _stats.vector_error = 0;
    _stats.queue_overflow_error = 0;
    _stats.min_stack_size = 0;
    ZH_LOGI("Error statistic reset successfully.");
}

static esp_err_t _zh_pcf8575_validate_config(const zh_pcf8575_init_config_t *config) // -V2008
{
    ZH_ERROR_CHECK(config->i2c_address >= 0x20 && config->i2c_address <= 0x27, ESP_ERR_INVALID_ARG, NULL, "Invalid I2C address.");
    ZH_ERROR_CHECK(config->i2c_frequency <= 400000, ESP_ERR_INVALID_ARG, NULL, "Invalid I2C frequency.");
    ZH_ERROR_CHECK(config->task_priority >= 1 && config->stack_size >= configMINIMAL_STACK_SIZE, ESP_ERR_INVALID_ARG, NULL, "Invalid task settings.");
    ZH_ERROR_CHECK(config->interrupt_gpio <= GPIO_NUM_MAX, ESP_ERR_INVALID_ARG, NULL, "Invalid GPIO number.");
    ZH_ERROR_CHECK(config->i2c_handle != NULL, ESP_ERR_INVALID_ARG, NULL, "Invalid I2C handle.");
    for (uint8_t i = 0; i < sizeof(_i2c_matrix); ++i)
    {
        ZH_ERROR_CHECK(config->i2c_address != _i2c_matrix[i], ESP_ERR_INVALID_ARG, NULL, "I2C address already present.");
    }
    return ESP_OK;
}

static esp_err_t _zh_pcf8575_gpio_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle) // -V2008
{
    uint16_t reg_temp = 0;
    if (_interrupt_gpio != GPIO_NUM_MAX)
    {
        ZH_ERROR_CHECK(_zh_pcf8575_read_register(handle, &reg_temp) == ESP_OK, ESP_FAIL, NULL, "Failed to read expander register.");
        ZH_ERROR_CHECK(zh_vector_push_back(&_vector, handle) == ESP_OK, ESP_FAIL, NULL, "Failed add item to vector.")
        return ESP_OK;
    }
    ZH_ERROR_CHECK(zh_vector_init(&_vector, sizeof(zh_pcf8575_handle_t)) == ESP_OK, ESP_FAIL, NULL, "Failed create vector.")
    ZH_ERROR_CHECK(_zh_pcf8575_read_register(handle, &reg_temp) == ESP_OK, ESP_FAIL, NULL, "Failed to read expander register.");
    ZH_ERROR_CHECK(zh_vector_push_back(&_vector, handle) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed."), "Failed add item to vector.")
    gpio_config_t interrupt_gpio_config = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << config->interrupt_gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ZH_ERROR_CHECK(gpio_config(&interrupt_gpio_config) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed."), "GPIO configuration failed.")
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    ZH_ERROR_CHECK(err == ESP_OK || err == ESP_ERR_INVALID_STATE, ESP_FAIL,
                   ZH_ERROR_CHECK(gpio_reset_pin(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Reset gpio failed.");
                   ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed."), "Failed install isr service.")
    ZH_ERROR_CHECK(gpio_isr_handler_add(config->interrupt_gpio, _zh_pcf8575_isr_handler, NULL) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(gpio_reset_pin(config->interrupt_gpio) == ESP_OK, ESP_FAIL, NULL, "Reset gpio failed.");
                   ZH_ERROR_CHECK(zh_vector_free(&_vector) == ESP_OK, ESP_FAIL, NULL, "Free vector failed."), "Failed add isr handler.")
    _interrupt_gpio = config->interrupt_gpio;
    return ESP_OK;
}

static esp_err_t _zh_pcf8575_i2c_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle)
{
    i2c_device_config_t pcf8575_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_address,
        .scl_speed_hz = config->i2c_frequency,
    };
    i2c_master_dev_handle_t _dev_handle = NULL;
    ZH_ERROR_CHECK(i2c_master_bus_add_device(config->i2c_handle, &pcf8575_config, &_dev_handle) == ESP_OK, ESP_FAIL, NULL, "Failed to add I2C device.");
    ZH_ERROR_CHECK(i2c_master_probe(config->i2c_handle, config->i2c_address, 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL,
                   ZH_ERROR_CHECK(i2c_master_bus_rm_device(_dev_handle) == ESP_OK, ESP_FAIL, NULL, "I2C remove device failed."), "Expander not connected or not responding.");
    handle->dev_handle = _dev_handle;
    handle->gpio_work_mode = (config->p17_gpio_work_mode << 15) | (config->p16_gpio_work_mode << 14) | (config->p15_gpio_work_mode << 13) |
                             (config->p14_gpio_work_mode << 12) | (config->p13_gpio_work_mode << 11) | (config->p12_gpio_work_mode << 10) |
                             (config->p11_gpio_work_mode << 9) | (config->p10_gpio_work_mode << 8) | (config->p07_gpio_work_mode << 7) |
                             (config->p06_gpio_work_mode << 6) | (config->p05_gpio_work_mode << 5) | (config->p04_gpio_work_mode << 4) |
                             (config->p03_gpio_work_mode << 3) | (config->p02_gpio_work_mode << 2) | (config->p01_gpio_work_mode << 1) |
                             (config->p00_gpio_work_mode << 0);
    handle->gpio_status = handle->gpio_work_mode;
    handle->i2c_address = config->i2c_address;
    return ESP_OK;
}

static esp_err_t _zh_pcf8575_resources_init(const zh_pcf8575_init_config_t *config)
{
    _interrupt_semaphore = xSemaphoreCreateBinary();
    ZH_ERROR_CHECK(_interrupt_semaphore != NULL, ESP_ERR_NO_MEM, NULL, "Failed to create semaphore.")
    return ESP_OK;
}

static esp_err_t _zh_pcf8575_task_init(const zh_pcf8575_init_config_t *config)
{
    ZH_ERROR_CHECK(xTaskCreatePinnedToCore(&_zh_pcf8575_isr_processing_task, "zh_pcf8575_isr_processing_task", config->stack_size, NULL, config->task_priority, &zh_pcf8575, tskNO_AFFINITY) == pdPASS,
                   ESP_FAIL, NULL, "Failed to create isr processing task.")
    return ESP_OK;
}

static void IRAM_ATTR _zh_pcf8575_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xSemaphoreGiveFromISR(_interrupt_semaphore, &xHigherPriorityTaskWoken) != pdTRUE)
    {
        ++_stats.queue_overflow_error;
    }
    if (xHigherPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    };
}

static void IRAM_ATTR _zh_pcf8575_isr_processing_task(void *pvParameter)
{
    for (;;)
    {
        xSemaphoreTake(_interrupt_semaphore, portMAX_DELAY);
        int16_t vector_size = (int16_t)zh_vector_get_size(&_vector);
        if (vector_size == ESP_FAIL)
        {
            ++_stats.vector_error;
            ZH_LOGE("PCF8575 isr processing failed. Failed to get vector size.", ESP_FAIL);
        }
        else
        {
            for (uint16_t i = 0; i < vector_size; ++i)
            {
                zh_pcf8575_handle_t *handle = zh_vector_get_item(&_vector, i);
                if (handle == NULL)
                {
                    ++_stats.vector_error;
                    ZH_LOGE("PCF8575 isr processing failed. Failed to get vector item data.", ESP_FAIL);
                    continue;
                }
                zh_pcf8575_event_on_isr_t event = {0};
                event.i2c_address = handle->i2c_address;
                uint16_t old_reg = handle->gpio_status;
                uint16_t new_reg = 0;
                esp_err_t err = _zh_pcf8575_read_register(handle, &new_reg);
                if (err != ESP_OK)
                {
                    ZH_LOGE("PCF8575 isr processing failed. Failed to read expander register.", err);
                    continue;
                }
                for (uint8_t j = 0; j <= 15; ++j)
                {
                    if ((handle->gpio_work_mode & _gpio_matrix[j]) != 0)
                    {
                        if ((old_reg & _gpio_matrix[j]) != (new_reg & _gpio_matrix[j]))
                        {
                            event.gpio_number = j;
                            event.gpio_level = new_reg & _gpio_matrix[j];
                            event.interrupt_time = esp_timer_get_time();
                            err = esp_event_post(ZH_PCF8575, 0, &event, sizeof(event), 1000 / portTICK_PERIOD_MS);
                            if (err != ESP_OK)
                            {
                                ++_stats.event_post_error;
                                ZH_LOGE("PCF8575 isr processing failed. Failed to post interrupt event.", err);
                                continue;
                            }
                        }
                    }
                }
            }
        }
        _stats.min_stack_size = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    }
    vTaskDelete(NULL);
}

static esp_err_t _zh_pcf8575_read_register(zh_pcf8575_handle_t *handle, uint16_t *reg)
{
    ZH_ERROR_CHECK(i2c_master_receive(handle->dev_handle, (uint8_t *)&handle->gpio_status, sizeof(handle->gpio_status), 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL,
                   ++_stats.i2c_driver_error, "I2C driver error.");
    *reg = handle->gpio_status;
    return ESP_OK;
}

static esp_err_t _zh_pcf8575_write_register(zh_pcf8575_handle_t *handle, uint16_t reg)
{
    ZH_ERROR_CHECK(i2c_master_transmit(handle->dev_handle, (uint8_t *)&reg, sizeof(reg), 1000 / portTICK_PERIOD_MS) == ESP_OK, ESP_FAIL,
                   ++_stats.i2c_driver_error, "I2C driver error.");
    handle->gpio_status = reg;
    return ESP_OK;
}