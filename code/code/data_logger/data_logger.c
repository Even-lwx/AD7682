#include "data_logger.h"
#include "w25q128.h"
#include <string.h>

#define LOGGER_META_ADDRESS     0x00FFF000UL
#define LOGGER_DATA_END         LOGGER_META_ADDRESS
#define LOGGER_RECORD_SIZE      64U
#define LOGGER_META_SIZE        32U
#define LOGGER_SECTOR_SIZE      4096U
#define LOGGER_MAX_RECORDS      (LOGGER_DATA_END / LOGGER_RECORD_SIZE)
#define LOGGER_META_SLOTS       (LOGGER_SECTOR_SIZE / LOGGER_META_SIZE)
#define LOGGER_RECORD_MAGIC     0x4C47U
#define LOGGER_META_MAGIC       0x4D47U
#define LOGGER_VERSION          1U
#define LOGGER_DEFAULT_INTERVAL 60UL

static SPI_HandleTypeDef *g_logger_spi;
static uint32_t g_logger_head;
static uint32_t g_logger_count;
static uint32_t g_logger_sequence;
static uint32_t g_logger_meta_slot;
static uint32_t g_logger_interval = LOGGER_DEFAULT_INTERVAL;
static uint8_t g_logger_dump_newest_first;

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

static void Logger_EncodeSample(const DataLogger_Sample *sample,
                                uint32_t sequence, uint8_t record[LOGGER_RECORD_SIZE])
{
    int32_t temperature = (int32_t)(sample->temperature_c * 100.0f);
    uint32_t humidity = (uint32_t)(sample->humidity_rh * 100.0f);
    int32_t pressure_u7 = (int32_t)(sample->pressure_u7_hpa * 100.0f);
    int32_t pressure_u11 = (int32_t)(sample->pressure_u11_hpa * 100.0f);

    (void)memset(record, 0xFF, LOGGER_RECORD_SIZE);
    Logger_Put16(&record[0], LOGGER_RECORD_MAGIC);
    record[2] = LOGGER_VERSION;
    Logger_Put32(&record[4], sequence);
    record[8] = sample->datetime.century;
    record[9] = sample->datetime.year;
    record[10] = sample->datetime.month;
    record[11] = sample->datetime.day;
    record[12] = sample->datetime.hours;
    record[13] = sample->datetime.minutes;
    record[14] = sample->datetime.seconds;
    Logger_Put32(&record[16], (uint32_t)temperature);
    Logger_Put32(&record[20], humidity);
    Logger_Put32(&record[24], (uint32_t)pressure_u7);
    Logger_Put32(&record[28], (uint32_t)pressure_u11);
    Logger_Put16(&record[32], sample->adc[0]);
    Logger_Put16(&record[34], sample->adc[1]);
    Logger_Put16(&record[36], sample->adc[2]);
    Logger_Put16(&record[38], sample->adc[3]);
    Logger_Put16(&record[62], Logger_Crc16(record, 62U));
}

static HAL_StatusTypeDef Logger_DecodeSample(const uint8_t record[LOGGER_RECORD_SIZE],
                                             DataLogger_Sample *sample)
{
    if (Logger_Get16(&record[0]) != LOGGER_RECORD_MAGIC ||
        record[2] != LOGGER_VERSION ||
        Logger_Get16(&record[62]) != Logger_Crc16(record, 62U))
    {
        return HAL_ERROR;
    }
    sample->datetime.century = record[8];
    sample->datetime.year = record[9];
    sample->datetime.month = record[10];
    sample->datetime.day = record[11];
    sample->datetime.hours = record[12];
    sample->datetime.minutes = record[13];
    sample->datetime.seconds = record[14];
    sample->temperature_c = (float)(int32_t)Logger_Get32(&record[16]) / 100.0f;
    sample->humidity_rh = (float)Logger_Get32(&record[20]) / 100.0f;
    sample->pressure_u7_hpa = (float)(int32_t)Logger_Get32(&record[24]) / 100.0f;
    sample->pressure_u11_hpa = (float)(int32_t)Logger_Get32(&record[28]) / 100.0f;
    sample->adc[0] = Logger_Get16(&record[32]);
    sample->adc[1] = Logger_Get16(&record[34]);
    sample->adc[2] = Logger_Get16(&record[36]);
    sample->adc[3] = Logger_Get16(&record[38]);
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
    Logger_Put32(&meta[20], g_logger_interval);
    meta[24] = g_logger_dump_newest_first;
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

    if (spi == NULL)
    {
        return HAL_ERROR;
    }
    g_logger_spi = spi;
    g_logger_head = 0U;
    g_logger_count = 0U;
    g_logger_sequence = 0U;
    g_logger_meta_slot = 0U;
    g_logger_interval = LOGGER_DEFAULT_INTERVAL;
    g_logger_dump_newest_first = 0U;

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
        if (Logger_Get32(&meta[20]) != 0xFFFFFFFFUL && Logger_Get32(&meta[20]) != 0U)
        {
            g_logger_interval = Logger_Get32(&meta[20]);
        }
        if (meta[24] != 0xFFU)
        {
            g_logger_dump_newest_first = (meta[24] != 0U) ? 1U : 0U;
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
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Append(const DataLogger_Sample *sample,
                                    uint32_t timeout)
{
    uint8_t record[LOGGER_RECORD_SIZE];
    uint32_t address;

    if (g_logger_spi == NULL || sample == NULL)
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
    g_logger_interval = LOGGER_DEFAULT_INTERVAL;
    g_logger_dump_newest_first = 0U;
    return HAL_OK;
}

HAL_StatusTypeDef DataLogger_SetInterval(uint32_t seconds, uint32_t timeout)
{
    if (seconds == 0U || seconds > 86400UL || g_logger_spi == NULL)
    {
        return HAL_ERROR;
    }
    g_logger_interval = seconds;
    return Logger_WriteMetadata(timeout);
}

HAL_StatusTypeDef DataLogger_SetDumpNewestFirst(uint8_t newest_first,
                                                uint32_t timeout)
{
    if (g_logger_spi == NULL)
    {
        return HAL_ERROR;
    }
    g_logger_dump_newest_first = (newest_first != 0U) ? 1U : 0U;
    return Logger_WriteMetadata(timeout);
}

uint32_t DataLogger_Count(void)
{
    return g_logger_count;
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
