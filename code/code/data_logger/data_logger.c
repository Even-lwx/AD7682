#include "data_logger.h"
#include "w25q128.h"
#include <string.h>

#define LOGGER_META_ADDRESS     0x00FFF000UL
#define LOGGER_DATA_END         LOGGER_META_ADDRESS
#define LOGGER_RECORD_SIZE      32U
#define LOGGER_META_SIZE        32U
#define LOGGER_SECTOR_SIZE      4096U
#define LOGGER_MAX_RECORDS      (LOGGER_DATA_END / LOGGER_RECORD_SIZE)
#define LOGGER_META_SLOTS       (LOGGER_SECTOR_SIZE / LOGGER_META_SIZE)
#define LOGGER_RECORD_MAGIC     0x4C47U
#define LOGGER_META_MAGIC       0x4D47U
#define LOGGER_VERSION          2U
#define LOGGER_FRACTION_VERSION 3U
#define LOGGER_DEFAULT_INTERVAL 60UL

/* STM32G031C8 内部 Flash 为 64 KiB，最后 2 KiB 页面用于保存日志配置；
 * MDK 的 IROM 区域已相应缩短。 */
#define LOGGER_CONFIG_ADDRESS   0x0800F800UL
#define LOGGER_CONFIG_SIZE      32U
#define LOGGER_CONFIG_SLOTS     (FLASH_PAGE_SIZE / LOGGER_CONFIG_SIZE)
#define LOGGER_CONFIG_MAGIC     0x4346U
#define LOGGER_CONFIG_VERSION   1U

static SPI_HandleTypeDef *g_logger_spi;
static uint32_t g_logger_head;
static uint32_t g_logger_count;
static uint32_t g_logger_sequence;
static uint32_t g_logger_meta_slot;
static uint32_t g_logger_interval = LOGGER_DEFAULT_INTERVAL;
static uint8_t g_logger_dump_newest_first;
static uint8_t g_logger_recording_enabled = 1U;
static uint32_t g_logger_config_slot;
static uint32_t g_logger_config_sequence;

/* 32 字节记录布局：
 * 0~1 魔数，2 版本/ADC小数，3 ADC小数，4~7 序号，8~13 RTC时间/ADC小数，
 * 14~15 温度（0.01 °C），16~17 湿度（0.01 %RH），18~19 U7 压力（0.1 hPa），
 * 20~21 U11 压力（0.1 hPa），22~29 ADC0~ADC3，30~31 CRC16。
 * 无效的温湿度/压力分别使用 0x8000、0xFFFF、0x8000 表示。
 * 记录长度为 32 字节，可整除 4 KB 扇区，避免记录跨扇区。 */

static uint16_t Logger_Crc16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U)
                              : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

static void Logger_Put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void Logger_Put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U);
    p[3] = (uint8_t)(value >> 24U);
}

static uint16_t Logger_Get16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t Logger_Get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static int16_t Logger_EncodeTemperature(float value)
{
    if (!(value >= -327.67f && value <= 327.67f))
    {
        return (int16_t)-32768;
    }
    return (int16_t)(value * 100.0f);
}

static uint16_t Logger_EncodeHumidity(float value)
{
    if (!(value >= 0.0f && value <= 655.34f))
    {
        return 0xFFFFU;
    }
    return (uint16_t)(value * 100.0f);
}

static int16_t Logger_EncodePressure(float value)
{
    if (!(value >= -3276.7f && value <= 3276.6f))
    {
        return (int16_t)-32768;
    }
    return (int16_t)(value * 10.0f);
}

static uint8_t Logger_ConfigValid(const uint8_t config[LOGGER_CONFIG_SIZE])
{
    return (uint8_t)(Logger_Get16(&config[0]) == LOGGER_CONFIG_MAGIC &&
                     config[2] == LOGGER_CONFIG_VERSION &&
                     Logger_Get16(&config[30]) == Logger_Crc16(config, 30U));
}

static uint8_t Logger_ConfigErased(const uint8_t config[LOGGER_CONFIG_SIZE])
{
    uint32_t i;
    for (i = 0U; i < LOGGER_CONFIG_SIZE; i++)
    {
        if (config[i] != 0xFFU) { return 0U; }
    }
    return 1U;
}

static uint8_t Logger_LoadConfig(void)
{
    const uint8_t *config;
    uint32_t slot;
    uint32_t latest_slot = 0U;
    uint8_t found = 0U;
    uint8_t free_slot_found = 0U;

    g_logger_interval = LOGGER_DEFAULT_INTERVAL;
    g_logger_dump_newest_first = 0U;
    g_logger_recording_enabled = 1U;
    g_logger_config_slot = 0U;
    g_logger_config_sequence = 0U;
    for (slot = 0U; slot < LOGGER_CONFIG_SLOTS; slot++)
    {
        config = (const uint8_t *)(LOGGER_CONFIG_ADDRESS +
                                   slot * LOGGER_CONFIG_SIZE);
        if (Logger_ConfigValid(config))
        {
            g_logger_config_sequence = Logger_Get32(&config[4]);
            if (Logger_Get32(&config[8]) != 0U &&
                Logger_Get32(&config[8]) <= 86400UL)
            {
                g_logger_interval = Logger_Get32(&config[8]);
            }
            if (config[12] <= 1U)
            {
                g_logger_dump_newest_first = config[12];
            }
            /* Reserved byte in v1: legacy 0xFF keeps automatic logging on. */
            g_logger_recording_enabled = config[13] <= 1U ? config[13] : 1U;
            latest_slot = slot;
            found = 1U;
        }
        else if (free_slot_found == 0U && Logger_ConfigErased(config))
        {
            g_logger_config_slot = slot;
            free_slot_found = 1U;
        }
    }
    if (free_slot_found == 0U)
    {
        g_logger_config_slot = LOGGER_CONFIG_SLOTS;
    }
    else if (found != 0U && g_logger_config_slot <= latest_slot)
    {
        /* 掉电中断写入可能在第一个擦除槽之前留下损坏槽位。 */
        g_logger_config_slot = latest_slot + 1U;
    }
    return found;
}

static HAL_StatusTypeDef Logger_SaveConfig(void)
{
    uint8_t config[LOGGER_CONFIG_SIZE];
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    uint32_t address;
    uint32_t offset;
    uint64_t word;
    HAL_StatusTypeDef status;

    if (g_logger_config_slot >= LOGGER_CONFIG_SLOTS)
    {
        (void)memset(&erase, 0, sizeof(erase));
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Banks = FLASH_BANK_1;
        erase.Page = (LOGGER_CONFIG_ADDRESS - FLASH_BASE) / FLASH_PAGE_SIZE;
        erase.NbPages = 1U;
        if (HAL_FLASH_Unlock() != HAL_OK)
        {
            return HAL_ERROR;
        }
        status = HAL_FLASHEx_Erase(&erase, &page_error);
        (void)HAL_FLASH_Lock();
        if (status != HAL_OK)
        {
            return status;
        }
        g_logger_config_slot = 0U;
    }

    (void)memset(config, 0xFF, sizeof(config));
    Logger_Put16(&config[0], LOGGER_CONFIG_MAGIC);
    config[2] = LOGGER_CONFIG_VERSION;
    g_logger_config_sequence++;
    Logger_Put32(&config[4], g_logger_config_sequence);
    Logger_Put32(&config[8], g_logger_interval);
    config[12] = g_logger_dump_newest_first;
    config[13] = g_logger_recording_enabled;
    Logger_Put16(&config[30], Logger_Crc16(config, 30U));
    address = LOGGER_CONFIG_ADDRESS + g_logger_config_slot * LOGGER_CONFIG_SIZE;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return HAL_ERROR;
    }
    status = HAL_OK;
    for (offset = 0U; offset < LOGGER_CONFIG_SIZE; offset += 8U)
    {
        (void)memcpy(&word, &config[offset], sizeof(word));
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                   address + offset, word);
        if (status != HAL_OK) { break; }
    }
    (void)HAL_FLASH_Lock();
    if (status == HAL_OK)
    {
        g_logger_config_slot++;
    }
    return status;
}

/* Fraction record v3 retains the v2 32-byte stride and sequence layout.
 * Four 7-bit hundredths (0..99) use 28 previously unused bits:
 * byte3[7:0], byte2[7:2], month[7:4], day[7:5], hours[7:5],
 * minutes[7:6], seconds[7:6]. Byte2[1:0]=3 identifies this layout.
 * Metadata stays v2, so existing logs remain discoverable and readable. */
static void Logger_PutFractions(uint8_t *r, uint32_t fractions)
{
    r[3] = (uint8_t)fractions;
    r[2] = (uint8_t)(LOGGER_FRACTION_VERSION | (((fractions >> 8U) & 63U) << 2U));
    r[9] = (uint8_t)((r[9] & 15U) | (((fractions >> 14U) & 15U) << 4U));
    r[10] = (uint8_t)((r[10] & 31U) | (((fractions >> 18U) & 7U) << 5U));
    r[11] = (uint8_t)((r[11] & 31U) | (((fractions >> 21U) & 7U) << 5U));
    r[12] = (uint8_t)((r[12] & 63U) | (((fractions >> 24U) & 3U) << 6U));
    r[13] = (uint8_t)((r[13] & 63U) | (((fractions >> 26U) & 3U) << 6U));
}

static uint32_t Logger_GetFractions(const uint8_t *r)
{
    return (uint32_t)r[3] | ((uint32_t)(r[2] >> 2U) << 8U) |
           ((uint32_t)(r[9] >> 4U) << 14U) |
           ((uint32_t)(r[10] >> 5U) << 18U) |
           ((uint32_t)(r[11] >> 5U) << 21U) |
           ((uint32_t)(r[12] >> 6U) << 24U) |
           ((uint32_t)(r[13] >> 6U) << 26U);
}

static void Logger_EncodeSample(const DataLogger_Sample *sample,
                                uint32_t sequence, uint8_t record[LOGGER_RECORD_SIZE])
{
    int16_t temperature = Logger_EncodeTemperature(sample->temperature_c);
    uint16_t humidity = Logger_EncodeHumidity(sample->humidity_rh);
    int16_t pressure_u7 = Logger_EncodePressure(sample->pressure_u7_hpa);
    int16_t pressure_u11 = Logger_EncodePressure(sample->pressure_u11_hpa);
    uint32_t fractions = 0U;
    uint32_t fixed;
    uint8_t channel;

    (void)memset(record, 0xFF, LOGGER_RECORD_SIZE);
    Logger_Put16(&record[0], LOGGER_RECORD_MAGIC);
    record[2] = LOGGER_VERSION;
    Logger_Put32(&record[4], sequence);
    record[8] = sample->datetime.year;
    record[9] = sample->datetime.month;
    record[10] = sample->datetime.day;
    record[11] = sample->datetime.hours;
    record[12] = sample->datetime.minutes;
    record[13] = sample->datetime.seconds;
    Logger_Put16(&record[14], (uint16_t)temperature);
    Logger_Put16(&record[16], humidity);
    Logger_Put16(&record[18], (uint16_t)pressure_u7);
    Logger_Put16(&record[20], (uint16_t)pressure_u11);
    for (channel = 0U; channel < 4U; channel++)
    {
        fixed = (sample->adc[channel] >= 0.0f && sample->adc[channel] <= 65535.0f) ?
                (uint32_t)((double)sample->adc[channel] * 100.0 + 0.5) : 6553500U;
        Logger_Put16(&record[22U + 2U * channel], (uint16_t)(fixed / 100U));
        fractions |= (fixed % 100U) << (7U * channel);
    }
    Logger_PutFractions(record, fractions);
    Logger_Put16(&record[30], Logger_Crc16(record, 30U));
}

static HAL_StatusTypeDef Logger_DecodeSample(const uint8_t record[LOGGER_RECORD_SIZE],
                                             DataLogger_Sample *sample)
{
    uint8_t fractional = (uint8_t)((record[2] & 3U) == LOGGER_FRACTION_VERSION);
    uint8_t channel;
    uint32_t fractions = fractional != 0U ? Logger_GetFractions(record) : 0U;
    if (Logger_Get16(&record[0]) != LOGGER_RECORD_MAGIC ||
        (record[2] != LOGGER_VERSION && fractional == 0U) ||
        Logger_Get16(&record[30]) != Logger_Crc16(record, 30U))
    {
        return HAL_ERROR;
    }
    sample->datetime.century = 0U;
    sample->datetime.year = record[8];
    sample->datetime.month = record[9] & 15U;
    sample->datetime.day = record[10] & 31U;
    sample->datetime.hours = record[11] & 31U;
    sample->datetime.minutes = record[12] & 63U;
    sample->datetime.seconds = record[13] & 63U;
    sample->temperature_c = ((int16_t)Logger_Get16(&record[14]) == (int16_t)-32768) ?
                            -9999.0f : (float)(int16_t)Logger_Get16(&record[14]) / 100.0f;
    sample->humidity_rh = (Logger_Get16(&record[16]) == 0xFFFFU) ?
                          -9999.0f : (float)Logger_Get16(&record[16]) / 100.0f;
    sample->pressure_u7_hpa = ((int16_t)Logger_Get16(&record[18]) == (int16_t)-32768) ?
                              -9999.0f : (float)(int16_t)Logger_Get16(&record[18]) / 10.0f;
    sample->pressure_u11_hpa = ((int16_t)Logger_Get16(&record[20]) == (int16_t)-32768) ?
                               -9999.0f : (float)(int16_t)Logger_Get16(&record[20]) / 10.0f;
    for (channel = 0U; channel < 4U; channel++)
    {
        uint32_t fraction = (fractions >> (7U * channel)) & 127U;
        if (fraction > 99U) { return HAL_ERROR; }
        sample->adc[channel] = (float)Logger_Get16(&record[22U + 2U * channel]) +
                               (float)fraction / 100.0f;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef Logger_WriteMetadata(uint32_t timeout)
{
    uint8_t meta[LOGGER_META_SIZE];
    uint32_t address;

    if (g_logger_meta_slot >= LOGGER_META_SLOTS)
    {
        if (W25Q128_EraseSector(g_logger_spi, LOGGER_META_ADDRESS, timeout) != HAL_OK)
        {
            return HAL_ERROR;
        }
        g_logger_meta_slot = 0U;
    }
    (void)memset(meta, 0xFF, sizeof(meta));
    Logger_Put16(&meta[0], LOGGER_META_MAGIC);
    meta[2] = LOGGER_VERSION;
    Logger_Put32(&meta[4], g_logger_head);
    Logger_Put32(&meta[8], g_logger_count);
    Logger_Put32(&meta[12], g_logger_sequence);
    Logger_Put32(&meta[16], g_logger_meta_slot);
    /* 记录间隔和导出顺序保存在 MCU 内部 Flash 配置页，不在 W25Q128 中。 */
    Logger_Put16(&meta[30], Logger_Crc16(meta, 30U));
    address = LOGGER_META_ADDRESS + g_logger_meta_slot * LOGGER_META_SIZE;
    if (W25Q128_Write(g_logger_spi, address, meta, sizeof(meta), timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    g_logger_meta_slot++;
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Init(SPI_HandleTypeDef *spi, uint32_t timeout)
{
    uint8_t meta[LOGGER_META_SIZE];
    uint32_t slot;
    uint32_t latest_slot = 0U;
    uint8_t found = 0U;
    uint8_t config_found;
    uint8_t legacy_config_found = 0U;

    if (spi == NULL)
    {
        return HAL_ERROR;
    }
    g_logger_spi = spi;
    g_logger_head = 0U;
    g_logger_count = 0U;
    g_logger_sequence = 0U;
    g_logger_meta_slot = 0U;
    config_found = Logger_LoadConfig();

    for (slot = 0U; slot < LOGGER_META_SLOTS; slot++)
    {
        if (W25Q128_Read(spi, LOGGER_META_ADDRESS + slot * LOGGER_META_SIZE,
                         meta, sizeof(meta), timeout) != HAL_OK ||
            Logger_Get16(&meta[0]) != LOGGER_META_MAGIC ||
            meta[2] != LOGGER_VERSION ||
            Logger_Get16(&meta[30]) != Logger_Crc16(meta, 30U))
        {
            break;
        }
        g_logger_head = Logger_Get32(&meta[4]);
        g_logger_count = Logger_Get32(&meta[8]);
        g_logger_sequence = Logger_Get32(&meta[12]);
        /* 仅在迁移旧版本时读取一次 W25Q128 中的旧配置。 */
        if (config_found == 0U &&
            Logger_Get32(&meta[20]) != 0xFFFFFFFFUL &&
            Logger_Get32(&meta[20]) != 0U)
        {
            g_logger_interval = Logger_Get32(&meta[20]);
            legacy_config_found = 1U;
        }
        if (config_found == 0U && meta[24] != 0xFFU)
        {
            g_logger_dump_newest_first = (meta[24] != 0U) ? 1U : 0U;
            legacy_config_found = 1U;
        }
        latest_slot = slot;
        found = 1U;
    }
    if (found != 0U)
    {
        g_logger_meta_slot = latest_slot + 1U;
        if (g_logger_count > LOGGER_MAX_RECORDS)
        {
            g_logger_count = LOGGER_MAX_RECORDS;
        }
        if (g_logger_head >= LOGGER_MAX_RECORDS)
        {
            g_logger_head = 0U;
        }
    }
    else
    {
        /* 新记录格式与旧版不兼容；首次写入前擦除元数据扇区，避免覆盖旧槽位。 */
        g_logger_meta_slot = LOGGER_META_SLOTS;
    }
    if (config_found == 0U && legacy_config_found != 0U)
    {
        /* 保留旧固件曾存放在 W25Q128 中的配置。 */
        (void)Logger_SaveConfig();
    }
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Append(const DataLogger_Sample *sample,
                                    uint32_t timeout)
{
    uint8_t record[LOGGER_RECORD_SIZE];
    uint32_t address;

    if (g_logger_spi == NULL || sample == NULL || g_logger_recording_enabled == 0U)
    {
        return HAL_ERROR;
    }
    if ((g_logger_head % (LOGGER_SECTOR_SIZE / LOGGER_RECORD_SIZE)) == 0U &&
        W25Q128_EraseSector(g_logger_spi,
                            g_logger_head * LOGGER_RECORD_SIZE, timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    g_logger_sequence++;
    Logger_EncodeSample(sample, g_logger_sequence, record);
    address = g_logger_head * LOGGER_RECORD_SIZE;
    if (W25Q128_Write(g_logger_spi, address, record, sizeof(record), timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    g_logger_head = (g_logger_head + 1U) % LOGGER_MAX_RECORDS;
    if (g_logger_count < LOGGER_MAX_RECORDS)
    {
        g_logger_count++;
    }
    return Logger_WriteMetadata(timeout);
}

HAL_StatusTypeDef DataLogger_Read(uint32_t ordinal,
                                  DataLogger_Sample *sample,
                                  uint32_t timeout)
{
    uint8_t record[LOGGER_RECORD_SIZE];
    uint32_t oldest;
    uint32_t index;

    if (g_logger_spi == NULL || sample == NULL || ordinal >= g_logger_count)
    {
        return HAL_ERROR;
    }
    oldest = (g_logger_head + LOGGER_MAX_RECORDS - g_logger_count) % LOGGER_MAX_RECORDS;
    index = (oldest + ordinal) % LOGGER_MAX_RECORDS;
    if (W25Q128_Read(g_logger_spi, index * LOGGER_RECORD_SIZE,
                     record, sizeof(record), timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return Logger_DecodeSample(record, sample);
}

HAL_StatusTypeDef DataLogger_Clear(uint32_t timeout)
{
    if (g_logger_spi == NULL ||
        W25Q128_EraseSector(g_logger_spi, LOGGER_META_ADDRESS, timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    g_logger_head = 0U;
    g_logger_count = 0U;
    g_logger_sequence = 0U;
    g_logger_meta_slot = 0U;
    /* 保留记录间隔和导出顺序；它们是 MCU Flash 中的用户配置。 */
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_SetInterval(uint32_t seconds, uint32_t timeout)
{
    uint32_t previous;
    HAL_StatusTypeDef status;

    if (seconds == 0U || seconds > 86400UL || g_logger_spi == NULL)
    {
        return HAL_ERROR;
    }
    previous = g_logger_interval;
    g_logger_interval = seconds;
    (void)timeout;
    status = Logger_SaveConfig();
    if (status != HAL_OK)
    {
        g_logger_interval = previous;
    }
    return status;
}

HAL_StatusTypeDef DataLogger_SetDumpNewestFirst(uint8_t newest_first,
                                                uint32_t timeout)
{
    uint8_t previous;
    HAL_StatusTypeDef status;

    if (g_logger_spi == NULL)
    {
        return HAL_ERROR;
    }
    previous = g_logger_dump_newest_first;
    g_logger_dump_newest_first = (newest_first != 0U) ? 1U : 0U;
    (void)timeout;
    status = Logger_SaveConfig();
    if (status != HAL_OK)
    {
        g_logger_dump_newest_first = previous;
    }
    return status;
}

uint32_t DataLogger_Count(void)
{
    return g_logger_count;
}

HAL_StatusTypeDef DataLogger_SetRecordingEnabled(uint8_t enabled)
{
    uint8_t previous = g_logger_recording_enabled;
    HAL_StatusTypeDef status;
    enabled = enabled != 0U ? 1U : 0U;
    if (enabled == previous) { return HAL_OK; }
    g_logger_recording_enabled = enabled;
    status = Logger_SaveConfig();
    if (status != HAL_OK) { g_logger_recording_enabled = previous; }
    return status;
}

uint8_t DataLogger_GetRecordingEnabled(void)
{
    return g_logger_recording_enabled;
}

uint32_t DataLogger_LastSequence(void)
{
    return g_logger_sequence;
}

HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t sequence,
                                         DataLogger_Sample *sample,
                                         uint32_t timeout)
{
    uint8_t record[LOGGER_RECORD_SIZE];
    uint32_t age = g_logger_sequence - sequence;
    uint32_t index;
    if (g_logger_spi == NULL || sample == NULL || age >= g_logger_count)
    {
        return HAL_ERROR;
    }
    index = (g_logger_head + LOGGER_MAX_RECORDS - 1U - age) % LOGGER_MAX_RECORDS;
    if (W25Q128_Read(g_logger_spi, index * LOGGER_RECORD_SIZE,
                    record, sizeof(record), timeout) != HAL_OK ||
        Logger_Get32(&record[4]) != sequence)
    {
        return HAL_ERROR;
    }
    return Logger_DecodeSample(record, sample);
}

uint32_t DataLogger_Capacity(void)
{
    return LOGGER_MAX_RECORDS;
}

uint32_t DataLogger_GetInterval(void)
{
    return g_logger_interval;
}

uint8_t DataLogger_GetDumpNewestFirst(void)
{
    return g_logger_dump_newest_first;
}
