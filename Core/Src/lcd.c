#include "lcd.h"
#include <string.h>

/* The backpack is a PCF8574 chip: 8 output pins we set over I2C.
   They are wired to the LCD like this:
     bit 0     = RS  (0 = command, 1 = character)
     bit 1     = RW  (always 0 = write)
     bit 2     = E   (the LCD reads its data pins when E goes high -> low)
     bit 3     = backlight
     bits 4..7 = data pins D4..D7 (the LCD takes 4 bits at a time) */
#define LCD_RS          0x01
#define LCD_E           0x04
#define LCD_BACKLIGHT   0x08

#define LCD_COLS        16
#define I2C_TIMEOUT_MS  10

static I2C_HandleTypeDef *lcd_i2c;
static uint16_t lcd_addr;                   /* 0 = no LCD found */
static char shown[2][LCD_COLS];             /* what is on the screen right now */

/* Put 4 bits on D4..D7, then pulse E so the LCD latches them. */
static void send_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t out = (uint8_t)(nibble << 4) | rs | LCD_BACKLIGHT;
    uint8_t seq[2] = { out | LCD_E, out };  /* E high, then E low */
    HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr, seq, 2, I2C_TIMEOUT_MS);
}

/* A full byte goes over as two halves, high half first. */
static void send_byte(uint8_t value, uint8_t rs)
{
    send_nibble(value >> 4, rs);
    send_nibble(value & 0x0F, rs);
}

static void command(uint8_t cmd)
{
    send_byte(cmd, 0);
}

int LCD_Init(I2C_HandleTypeDef *hi2c)
{
    lcd_i2c = hi2c;

    /* The LCD is the only chip on I2C3, so use whichever address answers.
       (Backpacks are usually 0x27 or 0x3F, but jumpers on them can change it.)
       HAL wants the 7-bit address shifted left by one. */
    for (uint16_t a = 0x08; a < 0x78 && !lcd_addr; a++)
        if (HAL_I2C_IsDeviceReady(hi2c, a << 1, 1, 2) == HAL_OK)
            lcd_addr = a << 1;

    if (!lcd_addr)
        return 0;                           /* no LCD: the synth still runs without it */

    /* The LCD powers up in 8-bit mode. This wake-up sequence from the
       HD44780 datasheet puts it into 4-bit mode from any state. */
    HAL_Delay(50);
    send_nibble(0x3, 0);  HAL_Delay(5);
    send_nibble(0x3, 0);  HAL_Delay(1);
    send_nibble(0x3, 0);  HAL_Delay(1);
    send_nibble(0x2, 0);                    /* now in 4-bit mode */

    command(0x28);                          /* 2 lines, 5x8 dot characters */
    command(0x0C);                          /* display on, cursor off */
    command(0x06);                          /* move right after each character */
    command(0x01);                          /* clear the screen */
    HAL_Delay(2);                           /* clearing takes 1.5 ms */

    memset(shown, ' ', sizeof shown);       /* the screen is all spaces now */
    return 1;
}

void LCD_PrintLine(int row, const char *text)
{
    char line[LCD_COLS];
    int i;

    if (!lcd_addr || row < 0 || row > 1)
        return;

    /* Copy the text, then pad with spaces so a shorter line erases the old one. */
    for (i = 0; i < LCD_COLS && text[i]; i++)
        line[i] = text[i];
    for (; i < LCD_COLS; i++)
        line[i] = ' ';

    if (memcmp(line, shown[row], LCD_COLS) == 0)
        return;                             /* nothing changed: skip the slow I2C */
    memcpy(shown[row], line, LCD_COLS);

    command(0x80 | (row ? 0x40 : 0x00));    /* move to the start of the row */
    for (i = 0; i < LCD_COLS; i++)
        send_byte((uint8_t)line[i], LCD_RS);
}
