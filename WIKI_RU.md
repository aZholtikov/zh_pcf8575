# zh_pcf8575 - Компонент 16-битного расширителя GPIO PCF8575 для ESP-IDF

## Содержание

- [Обзор](#обзор)
- [Возможности](#возможности)
- [Установка](#установка)
- [Справочник API](#справочник-api)
- [Примеры использования](#примеры-использования)
- [Технические характеристики](#технические-характеристики)
- [Коды ошибок](#коды-ошибок)
- [Вклад в проект](#вклад-в-проект)
- [Лицензия](#лицензия)

---

## Обзор

`zh_pcf8575` - это компонент ESP-IDF для 16-битных расширителей GPIO PCF8575. Он предоставляет удобный API для работы с расширителем GPIO через шину I2C. Компонент поддерживает до 8 расширителей на одной шине I2C и может генерировать прерывания при изменении состояния входных GPIO.

Компонент разработан специально для микроконтроллеров ESP32 и использует API драйвера I2C ESP-IDF v5.0+.

---

## Возможности

1. **Поддержка 8 расширителей**: До 8 PCF8575 на одной шине I2C (адреса 0x20-0x27)
2. **Режимы работы GPIO**: Поддержка выходных и входных режимов для каждого из 16 GPIO
3. **Генерация прерываний**: Поддержка прерываний от входных GPIO (отрицательный фронт)
4. **Статистика ошибок**: Встроенные счетчики ошибок I2C, событий, векторов и переполнения очереди
5. **Потокобезопасность**: Использует драйвер I2C ESP-IDF и FreeRTOS
6. **Минимальные накладные расходы**: Низкие требования к памяти и процессору
7. **Простой API**: Легкое чтение/запись состояния всех GPIO и отдельных пинов

---

## Установка

1. Перейдите в каталог компонентов вашего проекта:

```bash
cd ../ваш_проект/components
```

2. Клонируйте репозиторий:

```bash
git clone https://github.com/aZholtikov/zh_pcf8575.git
```

3. В вашем приложении подключите заголовочный файл:

```c
#include "zh_pcf8575.h"
```

4. Компонент будет автоматически собран вместе с вашим проектом.

### Обязательные настройки в menuconfig

Для корректной работы компонента включите следующие настройки в menuconfig:

```text
GPIO_CTRL_FUNC_IN_IRAM
I2C_ISR_IRAM_SAFE
I2C_MASTER_ISR_HANDLER_IN_IRAM
```

---

## Справочник API

### Макросы

```c
#define ZH_PCF8575_GPIO_OUTPUT false
#define ZH_PCF8575_GPIO_INPUT  true
#define ZH_PCF8575_GPIO_LOW    false
#define ZH_PCF8575_GPIO_HIGH   true
```

---

### Структура zh_pcf8575_init_config_t

```c
typedef struct
{
    i2c_master_bus_handle_t i2c_handle; // Уникальный дескриптор шины I2C
    uint32_t i2c_frequency;             // Частота I2C расширителя (макс. 400000 Гц)
    uint16_t stack_size;                // Размер стека задачи обработки прерываний
    uint8_t task_priority;              // Приоритет задачи обработки прерываний
    uint8_t i2c_address;                // Адрес устройства расширителя I2C (0x20-0x27)
    bool p00_gpio_work_mode;            // Режим работы GPIO P00 (false - output, true - input)
    bool p01_gpio_work_mode;            // Режим работы GPIO P01
    bool p02_gpio_work_mode;            // Режим работы GPIO P02
    bool p03_gpio_work_mode;            // Режим работы GPIO P03
    bool p04_gpio_work_mode;            // Режим работы GPIO P04
    bool p05_gpio_work_mode;            // Режим работы GPIO P05
    bool p06_gpio_work_mode;            // Режим работы GPIO P06
    bool p07_gpio_work_mode;            // Режим работы GPIO P07
    bool p10_gpio_work_mode;            // Режим работы GPIO P10
    bool p11_gpio_work_mode;            // Режим работы GPIO P11
    bool p12_gpio_work_mode;            // Режим работы GPIO P12
    bool p13_gpio_work_mode;            // Режим работы GPIO P13
    bool p14_gpio_work_mode;            // Режим работы GPIO P14
    bool p15_gpio_work_mode;            // Режим работы GPIO P15
    bool p16_gpio_work_mode;            // Режим работы GPIO P16
    bool p17_gpio_work_mode;            // Режим работы GPIO P17
    gpio_num_t interrupt_gpio;          // GPIO для прерываний (GPIO_NUM_MAX - отключить)
} zh_pcf8575_init_config_t;
```

Используйте макрос `ZH_PCF8575_INIT_CONFIG_DEFAULT()` для инициализации значениями по умолчанию:

- `i2c_frequency`: 400000 Гц
- `i2c_address`: 0xFF
- `p00_gpio_work_mode` ... `p17_gpio_work_mode`: `ZH_PCF8575_GPIO_OUTPUT`
- `interrupt_gpio`: `GPIO_NUM_MAX`

---

### Структура zh_pcf8575_handle_t

```c
typedef struct
{
    uint8_t i2c_address;                // Адрес I2C расширителя
    uint16_t gpio_work_mode;            // Режимы работы GPIO (биты 0-15)
    uint16_t gpio_status;               // Текущее состояние GPIO (биты 0-15)
    bool is_initialized;                // Флаг инициализации расширителя
    i2c_master_dev_handle_t dev_handle; // Уникальный дескриптор устройства I2C
    void *system;                       // Системный указатель для использования в других компонентах
} zh_pcf8575_handle_t;
```

---

### Структура zh_pcf8575_stats_t

```c
typedef struct
{
    uint32_t i2c_driver_error;     // Количество ошибок драйвера I2C
    uint32_t event_post_error;     // Количество ошибок отправки событий
    uint32_t vector_error;         // Количество ошибок вектора
    uint32_t queue_overflow_error; // Количество переполнений очереди
    uint32_t min_stack_size;       // Минимальный свободный стек задачи
} zh_pcf8575_stats_t;
```

---

### Структура zh_pcf8575_event_on_isr_t

```c
typedef struct
{
    uint64_t interrupt_time;           // Время прерывания (мкс)
    zh_pcf8575_gpio_num_t gpio_number; // Номер GPIO, вызвавшего прерывание
    uint8_t i2c_address;               // I2C адрес расширителя
    bool gpio_level;                   // Уровень GPIO при прерывании
} zh_pcf8575_event_on_isr_t;
```

---

### zh_pcf8575_init()

Инициализирует расширитель PCF8575.

**Параметры:**

- `config` - Указатель на структуру конфигурации инициализации PCF8575
- `handle` - Указатель на уникальный дескриптор PCF8575

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент (NULL config или handle)
- `ESP_ERR_INVALID_STATE` - Расширитель уже инициализирован
- `ESP_FAIL` - Ошибка инициализации

---

### zh_pcf8575_deinit()

Деинициализирует расширитель PCF8575.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент (NULL handle)
- `ESP_ERR_INVALID_STATE` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка деинициализации

---

### zh_pcf8575_read()

Считывает состояние всех GPIO расширителя.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575
- `reg` - Указатель для хранения состояния GPIO (биты 0-15)

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент (NULL handle или reg)
- `ESP_ERR_NOT_FOUND` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка связи I2C

**Примечание:** Для входных GPIO всегда будет 1 (HIGH).

---

### zh_pcf8575_write()

Устанавливает состояние всех выходных GPIO расширителя.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575
- `reg` - Состояние GPIO (биты 0-15)

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент (NULL handle)
- `ESP_ERR_NOT_FOUND` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка связи I2C

**Примечание:** Влияет только на выходные GPIO.

---

### zh_pcf8575_reset()

Сбрасывает все GPIO расширителя в начальное состояние.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент (NULL handle)
- `ESP_ERR_NOT_FOUND` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка связи I2C

---

### zh_pcf8575_read_gpio()

Считывает состояние одного GPIO расширителя.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575
- `gpio` - Номер GPIO (ZH_PCF8575_GPIO_NUM_P00 ... ZH_PCF8575_GPIO_NUM_P17)
- `status` - Указатель для хранения состояния GPIO (true - HIGH, false - LOW)

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент
- `ESP_ERR_NOT_FOUND` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка связи I2C

**Примечание:** Для входных GPIO всегда будет 1 (HIGH).

---

### zh_pcf8575_write_gpio()

Устанавливает состояние одного выходного GPIO расширителя.

**Параметры:**

- `handle` - Указатель на уникальный дескриптор PCF8575
- `gpio` - Номер GPIO (ZH_PCF8575_GPIO_NUM_P00 ... ZH_PCF8575_GPIO_NUM_P17)
- `status` - Состояние GPIO (true - HIGH, false - LOW)

**Возвращает:**

- `ESP_OK` - Успех
- `ESP_ERR_INVALID_ARG` - Неверный аргумент
- `ESP_ERR_NOT_FOUND` - Расширитель не инициализирован
- `ESP_FAIL` - Ошибка связи I2C

**Примечание:** Влияет только на выходные GPIO.

---

### zh_pcf8575_get_stats()

Получает статистику ошибок с момента последнего сброса.

**Возвращает:**

- Указатель на структуру статистики

**Пример:**

```c
const zh_pcf8575_stats_t *stats = zh_pcf8575_get_stats();
printf("Ошибки I2C: %ld\n", stats->i2c_driver_error);
```

---

### zh_pcf8575_reset_stats()

Сбрасывает счетчики статистики ошибок.

**Пример:**

```c
zh_pcf8575_reset_stats();
```

---

## Примеры использования

### Базовый пример: Один расширитель

```c
#include "zh_pcf8575.h"

#define I2C_PORT (I2C_NUM_MAX - 1)

zh_pcf8575_handle_t pcf8575_handle = {0};

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
    zh_pcf8575_init_config_t config = ZH_PCF8575_INIT_CONFIG_DEFAULT();
    config.i2c_handle = i2c_bus_handle;
    config.i2c_address = 0x20;
    config.p00_gpio_work_mode = ZH_PCF8575_GPIO_INPUT; // P00 - вход
    zh_pcf8575_init(&config, &pcf8575_handle);
    uint16_t reg = 0;
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("Статус GPIO: ", reg);
    printf("Установить P7 в 1, P1 в 1 и P0 в 0.\n");
    zh_pcf8575_write(&pcf8575_handle, 0b0000000010000010);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("Статус GPIO: ", reg);
    printf("Установить P0 в 0.\n");
    zh_pcf8575_write_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P00, ZH_PCF8575_GPIO_LOW);
    bool gpio = 0;
    zh_pcf8575_read_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P00, &gpio);
    printf("Статус P0: %d.\n", gpio);
    printf("Установить P1 в 0.\n");
    zh_pcf8575_write_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P01, ZH_PCF8575_GPIO_LOW);
    zh_pcf8575_read_gpio(&pcf8575_handle, ZH_PCF8575_GPIO_NUM_P01, &gpio);
    printf("Статус P1: %d.\n", gpio);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("Статус GPIO: ", reg);
    printf("Сброс всех GPIO.\n");
    zh_pcf8575_reset(&pcf8575_handle);
    zh_pcf8575_read(&pcf8575_handle, &reg);
    print_gpio_status("Статус GPIO: ", reg);
    const zh_pcf8575_stats_t *stats = zh_pcf8575_get_stats();
    printf("Ошибки I2C: %ld.\n", stats->i2c_driver_error);
}
```

---

### Пример с прерываниями

```c
#include "zh_pcf8575.h"

#define I2C_PORT (I2C_NUM_MAX - 1)

zh_pcf8575_handle_t pcf8575_handle = {0};

void zh_pcf8575_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    zh_pcf8575_event_on_isr_t *event = event_data;
    printf("Прерывание на устройстве 0x%02X, GPIO %d, уровень %d, время %lld.\n",
           event->i2c_address, event->gpio_number, event->gpio_level, event->interrupt_time);
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
    config.p00_gpio_work_mode = ZH_PCF8575_GPIO_INPUT; // P00 - вход для прерываний
    config.p16_gpio_work_mode = ZH_PCF8575_GPIO_INPUT; // P16 - вход для прерываний
    config.interrupt_gpio = GPIO_NUM_14; // Подключение INT к GPIO 14
    zh_pcf8575_init(&config, &pcf8575_handle);
    for (;;)
    {
        const zh_pcf8575_stats_t *stats = zh_pcf8575_get_stats();
        printf("Ошибки I2C: %ld.\n", stats->i2c_driver_error);
        printf("Ошибки событий: %ld.\n", stats->event_post_error);
        printf("Очередь прерываний: %ld.\n", stats->queue_overflow_error);
        printf("Минимальный свободный стек: %ld.\n", stats->min_stack_size);
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}
```

---

## Технические характеристики

| Параметр | Значение |
|----------|----------|
| **Тип устройства** | 16-битный I2C GPIO расширитель |
| **Модель** | PCF8575 |
| **Количество устройств на шине** | До 8 (адреса 0x20-0x27) |
| **Количество GPIO** | 16 (P00-P17) |
| **Адрес I2C** | 0x20-0x27 |
| **Частота I2C** | До 400 кГц |
| **Режимы GPIO** | Вход/выход (настраиваемые для каждого пина) |
| **Прерывания** | Да (отрицательный фронт) |
| **Версия ESP-IDF** | >= 5.0 |
| **Платформа** | Семейство ESP32 |
| **Язык** | C (C99) |

---

## Коды ошибок

| Код ошибки | Описание |
|------------|----------|
| `ESP_OK` | Операция выполнена успешно |
| `ESP_ERR_INVALID_ARG` | Неверный аргумент (NULL указатель или неверная конфигурация) |
| `ESP_ERR_INVALID_STATE` | Устройство не инициализировано или уже инициализировано |
| `ESP_ERR_NOT_FOUND` | Устройство не инициализировано |
| `ESP_ERR_NO_MEM` | Недостаточно памяти |
| `ESP_FAIL` | Общая ошибка (ошибка связи I2C, инициализации и т.д.) |

---

## Вклад в проект

Вклад приветствуется! Чтобы внести свой вклад:

1. Сделайте форк репозитория
2. Создайте ветку функции (`git checkout -b feature/AmazingFeature`)
3. Закоммитьте ваши изменения (`git commit -m 'Add some AmazingFeature'`)
4. Отправьте в ветку (`git push origin feature/AmazingFeature`)
5. Откройте Pull Request

Пожалуйста, убедитесь, что ваш код следует существующему стилю и включает соответствующую документацию.

---

## Лицензия

Этот проект лицензирован по лицензии Apache, версия 2.0 - см. файл [LICENSE](LICENSE) для подробной информации.

### Apache License, Version 2.0

Авторское право (c) 2026 Алексей Жолтиков

Лицензировано по лицензии Apache License, Version 2.0 (далее — "Лицензия");
вы не можете использовать этот файл, кроме случаев, предусмотренных Лицензией.
Копию Лицензии можно получить по адресу:

    http://www.apache.org/licenses/LICENSE-2.0

Если иное не требуется действующим законодательством или не согласовано в письменном виде,
программное обеспечение, распространяемое по Лицензии, распространяется на условиях "КАК ЕСТЬ",
БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ, явных или подразумеваемых, включая, но не ограничиваясь, гарантии
ТОВАРНОГО СОСТОЯНИЯ, ПРИГОДНОСТИ ДЛЯ КОНКРЕТНОЙ ЦЕЛИ И НЕНАРУШЕНИЯ ПРАВ.
Смотрите Лицензию для получения конкретных прав и ограничений.

---

## Дополнительные заметки

- **Подтягивающие резисторы I2C**: Убедитесь, что к линиям SDA и SCL подключены подтягивающие резисторы (обычно 4.7кОм)
- **Входные GPIO**: Все входные GPIO всегда подтянуты к питанию (внутренний подтягивающий резистор ~100кОм)
- **Прерывания**: Все выводы INT расширителей должны быть подключены к одному GPIO на ESP32
- **I2C_ISR_IRAM_SAFE**: Для правильной работы включите `I2C_ISR_IRAM_SAFE` и `I2C_MASTER_ISR_HANDLER_IN_IRAM` в menuconfig
- **GPIO_CTRL_FUNC_IN_IRAM**: Для работы прерываний GPIO включите `GPIO_CTRL_FUNC_IN_IRAM` в menuconfig

---

*Сгенерировано для zh_pcf8575 v1.0.2*
