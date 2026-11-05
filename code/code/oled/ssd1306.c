#include "ssd1306.h"
#include "w25q128.h"
#include <string.h>

/* SSD1306 Rev1.1: 8.1.3 four-wire SPI, 10.1 commands, 13.4 timing;
 * charge-pump application note: 8D/14 enables the module's internal pump.
 * SPI mode 0, MSB first, 2 MHz shared with W25Q128. No MISO/ACK exists:
 * HAL_OK confirms transmission only, not physical presence of the display. */
#define OLED_CS GPIO_PIN_0
#define OLED_DC GPIO_PIN_1
#define OLED_RES GPIO_PIN_10
#define OLED_IDLE 8U
#define OLED_FRAME_PERIOD_MS 100U /* Target 10 Hz, subject to main-loop load. */
#define OLED_PAGE_PERIOD_MS 1U
static SPI_HandleTypeDef *g_spi;
static char g_lines[8][22];
static uint8_t g_page = OLED_IDLE;
static uint8_t g_ready;
static uint8_t g_large_text;
static uint32_t g_frame_tick;
static uint32_t g_page_tick;

/* Five columns per glyph, low bit at top. Sixth column is spacing. */
static const char g_chars[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+/%";
static const uint8_t g_font[][5] = {
 {0,0,0,0,0},
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},
 {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
 {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},
 {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},
 {0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},
 {0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
 {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},
 {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},
 {0,0x60,0x60,0,0},{8,8,8,8,8},{0,0x36,0x36,0,0},
 {8,8,0x3e,8,8},{0x20,0x10,8,4,2},{0x23,0x13,8,0x64,0x62}
};

static HAL_StatusTypeDef Send(uint8_t data, const uint8_t *bytes, uint16_t count)
{
 HAL_StatusTypeDef status;
 /* Never deselect another owner's live transaction. All callers are main-loop. */
 if (g_spi == NULL || HAL_SPI_GetState(g_spi) != HAL_SPI_STATE_READY ||
     HAL_GPIO_ReadPin(W25Q128_CS_PORT, W25Q128_CS_PIN) == GPIO_PIN_RESET)
 { return HAL_BUSY; }
 HAL_GPIO_WritePin(GPIOB, OLED_DC, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
 HAL_GPIO_WritePin(GPIOB, OLED_CS, GPIO_PIN_RESET);
 /* TIM3 ADC sampling may preempt this call for several milliseconds. */
 status = HAL_SPI_Transmit(g_spi, (uint8_t *)bytes, count, 25U);
 HAL_GPIO_WritePin(GPIOB, OLED_CS, GPIO_PIN_SET);
 return status;
}

static HAL_StatusTypeDef WritePage(uint8_t page, const uint8_t pixels[128])
{
 uint8_t address[3] = {0xb0U, 0x00U, 0x10U};
 HAL_StatusTypeDef status;
 address[0] |= page;
 status = Send(0U, address, sizeof(address));
 if (status == HAL_OK) { status = Send(1U, pixels, 128U); }
 return status;
}

HAL_StatusTypeDef SSD1306_Init(SPI_HandleTypeDef *spi)
{
 /* 128x64, page addressing, alternate COM, internal charge pump.
  * Moderate contrast reduces OLED load; no scrolling. */
 static const uint8_t init[] = {
  0xae,0xd5,0x80,0xa8,0x3f,0xd3,0x00,0x40,0x8d,0x14,
  0x20,0x02,0xa1,0xc8,0xda,0x12,0x81,0x3f,0xd9,0xf1,
  0xdb,0x20,0xa4,0xa6,0x2e
 };
 uint8_t pixels[128] = {0};
 uint8_t page;
 uint8_t on = 0xafU;
 HAL_StatusTypeDef status;
 g_ready = 0U;
 g_page = OLED_IDLE;
 g_spi = spi;
 HAL_GPIO_WritePin(GPIOB, OLED_CS, GPIO_PIN_SET);
 HAL_Delay(100U);
 HAL_GPIO_WritePin(GPIOB, OLED_RES, GPIO_PIN_RESET);
 HAL_Delay(10U); /* Datasheet reset pulse minimum 3 us. */
 HAL_GPIO_WritePin(GPIOB, OLED_RES, GPIO_PIN_SET);
 HAL_Delay(20U);
 status = Send(0U, init, sizeof(init));
 if (status != HAL_OK) { return status; }
 for (page = 0U; page < 8U; page++)
 {
  status = WritePage(page, pixels);
  if (status != HAL_OK) { return status; }
 }
 status = Send(0U, &on, 1U);
 if (status == HAL_OK)
 {
  g_ready = 1U;
  g_frame_tick = HAL_GetTick() - OLED_FRAME_PERIOD_MS;
 }
 return status;
}

uint8_t SSD1306_BeginFrame(uint32_t now)
{
 if (!g_ready || g_page != OLED_IDLE || (uint32_t)(now-g_frame_tick) < OLED_FRAME_PERIOD_MS)
 { return 0U; }
 g_frame_tick = now;
 memset(g_lines, 0, sizeof(g_lines));
 g_page = 0U;
 return 1U;
}

void SSD1306_SetLine(uint8_t row, const char *text)
{
 uint8_t i;
 if (row >= 8U || text == NULL) { return; }
 for (i = 0U; i < 21U && text[i] != 0; i++) { g_lines[row][i] = text[i]; }
 g_lines[row][i] = 0;
}

void SSD1306_BeginUiFrame(uint32_t now)
{
 if (!g_ready) { return; }
 g_large_text = 0U;
 memset(g_lines, 0, sizeof(g_lines));
 g_frame_tick = now;
 g_page = 0U;
}

void SSD1306_SetLargeText(uint8_t enabled)
{
 g_large_text = enabled != 0U;
}

void SSD1306_SetDashboard(void) { g_large_text = 2U; }

void SSD1306_Process(uint32_t now)
{
 uint8_t pixels[128] = {0};
 uint8_t col, glyph, row, x, bit, column_bits, py, sy, text_y;
 uint16_t expanded;
 const char *found;
 HAL_StatusTypeDef status;
 if (!g_ready || g_page == OLED_IDLE || (uint32_t)(now-g_page_tick) < OLED_PAGE_PERIOD_MS)
 { return; }
 if (g_large_text == 2U)
 {
  /* Five dashboard rows spread over the full 64-pixel height.  A 13-pixel
   * row pitch gives one more blank scanline between adjacent values than the
   * previous 12-pixel pitch, while keeping the same double-width glyph. */
  for (py=0U; py<8U; py++)
  {
   text_y=(uint8_t)(g_page*8U+py);
   row=(uint8_t)(text_y/13U);
   if (row >= 5U || (text_y%13U) >= 12U) { continue; }
   sy=(uint8_t)(((text_y%13U)*7U)/12U);
   for (col=0U;col<10U && g_lines[row][col]!=0;col++)
   {
    found=strchr(g_chars,g_lines[row][col]);
    glyph=found!=NULL?(uint8_t)(found-g_chars):0U;
    for (x=0U;x<5U;x++)
    {
     if (g_font[glyph][x] & (1U<<sy))
     {
      pixels[4U+col*12U+x*2U]|=(uint8_t)(1U<<py);
      pixels[5U+col*12U+x*2U]|=(uint8_t)(1U<<py);
     }
    }
   }
  }
 }
 else
 {
 row = g_large_text == 1U ? (uint8_t)(g_page / 2U) : g_page;
 for (col = 0U; col < (g_large_text == 1U ? 10U : 21U) && g_lines[row][col] != 0; col++)
 {
  found = strchr(g_chars, g_lines[row][col]);
  glyph = found != NULL ? (uint8_t)(found-g_chars) : 0U;
  if (g_large_text != 1U) { memcpy(&pixels[col*6U], g_font[glyph], 5U); }
  else
  {
   /* Expand each 5x8 glyph to 10x16, with two spacing columns.
    * Four-pixel left margin centers the 120-pixel text grid. */
   for (x = 0U; x < 5U; x++)
   {
    expanded = 0U;
    for (bit = 0U; bit < 8U; bit++)
    {
     if (g_font[glyph][x] & (1U << bit))
     { expanded |= (uint16_t)(3UL << (bit*2U)); }
    }
    column_bits = (uint8_t)(expanded >> ((g_page & 1U)*8U));
    pixels[4U+col*12U+x*2U] = column_bits;
    pixels[5U+col*12U+x*2U] = column_bits;
   }
  }
 }
 }
 status = WritePage(g_page, pixels);
 if (status == HAL_OK) { g_page++; }
 else if (status != HAL_BUSY)
 {
  /* Drop this frame after a bus error; next frame rewrites every page. */
  g_page = OLED_IDLE;
 }
 g_page_tick = now;
}
