#include "cs43l22.h"
#include "main.h"			/* for CODEC_RESET_Pin / CODEC_RESET_GPIO_Port */

/* Register addresses (CS43L22 datasheet, "Register Quick Reference") */
#define REG_ID				0x01
#define REG_POWER_CTL1		0x02
#define REG_POWER_CTL2		0x04
#define REG_CLOCKING_CTL	0x05
#define REG_INTERFACE_CTL1	0x06
#define REG_MASTER_A_VOL	0x20
#define REG_MASTER_B_VOL	0x21


static I2C_HandleTypeDef *codec_i2c;

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t value)
{
	return HAL_I2C_Mem_Write(codec_i2c, CS43L22_I2C_ADDR, reg,
							 I2C_MEMADD_SIZE_8BIT, &value, 1, 10);
}

static HAL_StatusTypeDef read_reg(uint8_t reg, uint8_t *value)
{
	return HAL_I2C_Mem_Read(codec_i2c, CS43L22_I2C_ADDR, reg,
							I2C_MEMADD_SIZE_8BIT, value, 1, 10);
}

HAL_StatusTypeDef CS43L22_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id, val;
    uint8_t err = 0;
    codec_i2c = hi2c;

    /* 1. Pulse the RESET pin (active low) to start from a clean state. */
    HAL_GPIO_WritePin(CODEC_RESET_GPIO_Port, CODEC_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(CODEC_RESET_GPIO_Port, CODEC_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    /* 2. Check the chip answers. The top 5 bits of the ID register are 11100. */
    if (read_reg(REG_ID, &id) != HAL_OK || (id & 0xF8) != 0xE0)
        return HAL_ERROR;

    /* 3. Stay powered down while we configure. */
    err |= write_reg(REG_POWER_CTL1, 0x01);

    /* 4. Headphone channels always on, speaker channels always off. */
    err |= write_reg(REG_POWER_CTL2, 0xAF);

    /* 5. Auto-detect the clock ratios from MCLK (bit 7), MCLK divide-by-2 (bit 0). */
    err |= write_reg(REG_CLOCKING_CTL, 0x81);

    /* 6. Slave mode, standard I2S (Philips) format. */
    err |= write_reg(REG_INTERFACE_CTL1, 0x04);

    /* 7. "Required initialization settings" from the datasheet (section 4.11).
          ST doesn't explain these; the datasheet just says to write them. */
    err |= write_reg(0x00, 0x99);
    err |= write_reg(0x47, 0x80);
    err |= read_reg(0x32, &val);
    err |= write_reg(0x32, val | 0x80);     /* set bit 7   */
    err |= write_reg(0x32, val & ~0x80);    /* clear bit 7 */
    err |= write_reg(0x00, 0x00);

    /* 8. Start quiet: -20 dB. A full-scale sine at 0 dB is painfully loud. */
    err |= CS43L22_SetVolume(0);

    return err ? HAL_ERROR : HAL_OK;
}


/* Call this AFTER the I2S DMA has started: the chip needs MCLK running
   before it powers up (datasheet, "Recommended Power-Up Sequence"). */
HAL_StatusTypeDef CS43L22_PowerOn(void)
{
    return write_reg(REG_POWER_CTL1, 0x9E);
}

/* Master volume in 0.5 dB steps: register value = dB * 2 (two's complement).
   0x00 = 0 dB, 0xD8 = -20 dB. We limit it to -100..0 dB. */
HAL_StatusTypeDef CS43L22_SetVolume(int8_t db)
{
    if (db > 0)    db = 0;
    if (db < -100) db = -100;

    uint8_t reg = (uint8_t)(int8_t)(db * 2);
    uint8_t err = 0;
    err |= write_reg(REG_MASTER_A_VOL, reg);    /* left  */
    err |= write_reg(REG_MASTER_B_VOL, reg);    /* right */
    return err ? HAL_ERROR : HAL_OK;
}
