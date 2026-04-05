/**
 * @file zh_pcf8575.h
 */

#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "zh_vector.h"

#define ZH_PCF8575_GPIO_OUTPUT false
#define ZH_PCF8575_GPIO_INPUT true
#define ZH_PCF8575_GPIO_LOW false
#define ZH_PCF8575_GPIO_HIGH true

/**
 * @brief PCF8575 expander initial default values.
 */
#define ZH_PCF8575_INIT_CONFIG_DEFAULT()              \
    {                                                 \
        .task_priority = 1,                           \
        .stack_size = configMINIMAL_STACK_SIZE,       \
        .i2c_address = 0xFF,                          \
        .i2c_frequency = 400000,                      \
        .p00_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p01_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p02_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p03_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p04_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p05_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p06_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p07_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p10_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p11_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p12_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p13_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p14_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p15_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p16_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .p17_gpio_work_mode = ZH_PCF8575_GPIO_OUTPUT, \
        .interrupt_gpio = GPIO_NUM_MAX}

#ifdef __cplusplus
extern "C"
{
#endif

    extern TaskHandle_t zh_pcf8575; /*!< Unique task handle. */

    /**
     * @brief Enumeration of PCF8575 expander GPIO.
     */
    typedef enum
    {
        ZH_PCF8575_GPIO_NUM_P00 = 0,
        ZH_PCF8575_GPIO_NUM_P01,
        ZH_PCF8575_GPIO_NUM_P02,
        ZH_PCF8575_GPIO_NUM_P03,
        ZH_PCF8575_GPIO_NUM_P04,
        ZH_PCF8575_GPIO_NUM_P05,
        ZH_PCF8575_GPIO_NUM_P06,
        ZH_PCF8575_GPIO_NUM_P07,
        ZH_PCF8575_GPIO_NUM_P10,
        ZH_PCF8575_GPIO_NUM_P11,
        ZH_PCF8575_GPIO_NUM_P12,
        ZH_PCF8575_GPIO_NUM_P13,
        ZH_PCF8575_GPIO_NUM_P14,
        ZH_PCF8575_GPIO_NUM_P15,
        ZH_PCF8575_GPIO_NUM_P16,
        ZH_PCF8575_GPIO_NUM_P17,
        ZH_PCF8575_GPIO_NUM_MAX
    } zh_pcf8575_gpio_num_t;

    /**
     * @brief Structure for initial initialization of PCF8575 expander.
     */
    typedef struct
    {
        i2c_master_bus_handle_t i2c_handle; /*!< Unique I2C bus handle. @attention Must be same for all PCF8575 expanders. */
        uint32_t i2c_frequency;             /*!< Expander I2C frequency. */
        uint16_t stack_size;                /*!< Stack size for task for the PCF8575 expander isr processing processing. @note The minimum size is configMINIMAL_STACK_SIZE. */
        uint8_t task_priority;              /*!< Task priority for the PCF8575 expander isr processing. @note Minimum value is 1. */
        uint8_t i2c_address;                /*!< Expander I2C address. */
        bool p00_gpio_work_mode;            /*!< Expander GPIO P0O work mode. */
        bool p01_gpio_work_mode;            /*!< Expander GPIO P01 work mode. */
        bool p02_gpio_work_mode;            /*!< Expander GPIO P02 work mode. */
        bool p03_gpio_work_mode;            /*!< Expander GPIO P03 work mode. */
        bool p04_gpio_work_mode;            /*!< Expander GPIO P04 work mode. */
        bool p05_gpio_work_mode;            /*!< Expander GPIO P05 work mode. */
        bool p06_gpio_work_mode;            /*!< Expander GPIO P06 work mode. */
        bool p07_gpio_work_mode;            /*!< Expander GPIO P07 work mode. */
        bool p10_gpio_work_mode;            /*!< Expander GPIO P10 work mode. */
        bool p11_gpio_work_mode;            /*!< Expander GPIO P11 work mode. */
        bool p12_gpio_work_mode;            /*!< Expander GPIO P12 work mode. */
        bool p13_gpio_work_mode;            /*!< Expander GPIO P13 work mode. */
        bool p14_gpio_work_mode;            /*!< Expander GPIO P14 work mode. */
        bool p15_gpio_work_mode;            /*!< Expander GPIO P15 work mode. */
        bool p16_gpio_work_mode;            /*!< Expander GPIO P16 work mode. */
        bool p17_gpio_work_mode;            /*!< Expander GPIO P17 work mode. */
        gpio_num_t interrupt_gpio;          /*!< Interrupt GPIO. @attention Must be same for all PCF8575 expanders. */
    } zh_pcf8575_init_config_t;

    /**
     * @brief PCF8575 expander handle.
     */
    typedef struct
    {
        uint8_t i2c_address;                /*!< Expander I2C address. */
        uint16_t gpio_work_mode;            /*!< Expander GPIO's work mode. */
        uint16_t gpio_status;               /*!< Expander GPIO's status. */
        bool is_initialized;                /*!< Expander initialization flag. */
        i2c_master_dev_handle_t dev_handle; /*!< Unique I2C device handle. */
        void *system;                       /*!< System pointer for use in another components. */
    } zh_pcf8575_handle_t;

    /**
     * @brief Structure for error statistics storage.
     */
    typedef struct
    {
        uint32_t i2c_driver_error;     /*!< Number of i2c driver error. */
        uint32_t event_post_error;     /*!< Number of event post error. */
        uint32_t vector_error;         /*!< Number of vector error. */
        uint32_t queue_overflow_error; /*!< Number of queue overflow error. */
        uint32_t min_stack_size;       /*!< Minimum free stack size. */
    } zh_pcf8575_stats_t;

    ESP_EVENT_DECLARE_BASE(ZH_PCF8575);

    /**
     * @brief Structure for sending data to the event handler when cause an interrupt.
     *
     * @note Should be used with ZH_PCF8575 event base.
     */
    typedef struct
    {
        uint64_t interrupt_time;           /*!< Interrupt time. */
        zh_pcf8575_gpio_num_t gpio_number; /*!< The GPIO that caused the interrupt. */
        uint8_t i2c_address;               /*!< The i2c address of PCF8575 expander that caused the interrupt. */
        bool gpio_level;                   /*!< The GPIO level that caused the interrupt. */
    } zh_pcf8575_event_on_isr_t;

    /**
     * @brief Initialize PCF8575 expander.
     *
     * @param[in] config Pointer to PCF8575 initialized configuration structure. Can point to a temporary variable.
     * @param[out] handle Pointer to unique PCF8575 handle.
     *
     * @attention I2C driver must be initialized first.
     *
     * @note Before initialize the expander recommend initialize zh_pcf8575_init_config_t structure with default values.
     *
     * @code zh_pcf8575_init_config_t config = ZH_PCF8575_INIT_CONFIG_DEFAULT() @endcode
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_init(const zh_pcf8575_init_config_t *config, zh_pcf8575_handle_t *handle);

    /**
     * @brief Deinitialize PCF8575 expander.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_deinit(zh_pcf8575_handle_t *handle);

    /**
     * @brief Read PCF8575 all GPIO's status.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     * @param[out] reg Pointer to GPIO's status.
     *
     * @note For input GPIO's status will be 1 (HIGH) always.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_read(zh_pcf8575_handle_t *handle, uint16_t *reg);

    /**
     * @brief Set PCF8575 all GPIO's status.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     * @param[in] reg GPIO's status.
     *
     * @attention Only the GPIO outputs are affected.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_write(zh_pcf8575_handle_t *handle, uint16_t reg);

    /**
     * @brief Reset (set to initial) PCF8575 all GPIO's.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_reset(zh_pcf8575_handle_t *handle);

    /**
     * @brief Read PCF8575 GPIO status.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     * @param[in] gpio GPIO number.
     * @param[out] status Pointer to GPIO status (true - HIGH, false - LOW).
     *
     * @note For input GPIO's status will be 1 (HIGH) always.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_read_gpio(zh_pcf8575_handle_t *handle, zh_pcf8575_gpio_num_t gpio, bool *status);

    /**
     * @brief Set PCF8575 GPIO status.
     *
     * @param[in] handle Pointer to unique PCF8575 handle.
     * @param[in] gpio GPIO number.
     * @param[in] status GPIO status (true - HIGH, false - LOW).
     *
     * @attention Only the GPIO output is affected.
     *
     * @return ESP_OK if success or an error code otherwise.
     */
    esp_err_t zh_pcf8575_write_gpio(zh_pcf8575_handle_t *handle, zh_pcf8575_gpio_num_t gpio, bool status);

    /**
     * @brief Get error statistics.
     *
     * @return Pointer to the statistics structure.
     */
    const zh_pcf8575_stats_t *zh_pcf8575_get_stats(void);

    /**
     * @brief Reset error statistics.
     */
    void zh_pcf8575_reset_stats(void);

#ifdef __cplusplus
}
#endif