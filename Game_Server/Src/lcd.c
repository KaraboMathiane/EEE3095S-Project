/* UCT WC1602A LCD, write-only 4-bit interface (R/W is grounded).
 * RS=PC14, E=PC15, D4=PB8, D5=PB9, D6=PA12, D7=PA15.
 * PC13 drives the backlight MOSFET. Pin mapping: supplied board schematic.
 */
#include "lcd.h"
#include "board_config.h"

#include <string.h>

static char displayed[2][17];
static uint8_t display_valid;

static void delay_us(uint32_t microseconds)
{
    /* Use the existing free-running SysTick without resetting it or reading
     * COUNTFLAG (the UART/button loop owns that flag). Handle counter wraps. */
    const uint32_t period = SYSTICK_LOAD_REG + 1U;
    uint32_t previous = SYSTICK_VAL_REG;
    uint32_t ticks = microseconds * (HSI_CLOCK_HZ / 1000000UL);
    while (ticks != 0U)
    {
        const uint32_t now = SYSTICK_VAL_REG;
        const uint32_t elapsed = previous >= now ? previous - now : previous + period - now;
        if (elapsed >= ticks) break;
        ticks -= elapsed;
        previous = now;
    }
}

static void nibble(uint8_t value)
{
    const uint32_t bmask = (1UL << 8U) | (1UL << 9U);
    const uint32_t amask = (1UL << 12U) | (1UL << 15U);
    uint32_t bset = ((uint32_t)value & 3U) << 8U;
    uint32_t aset = (((uint32_t)value & 4U) << 10U) |
                    (((uint32_t)value & 8U) << 12U);

    /* BSRR modifies only LCD pins, preserving USART and status LEDs. */
    GPIOB_BSRR_REG = bset | ((bmask & ~bset) << 16U);
    GPIOA_BSRR_REG = aset | ((amask & ~aset) << 16U);
    delay_us(2U);
    GPIOC_BSRR_REG = 1UL << 15U;
    delay_us(2U);
    GPIOC_BSRR_REG = 1UL << (15U + 16U);
    delay_us(2U);
}

static void write_byte(uint8_t value, uint8_t data)
{
    GPIOC_BSRR_REG = data ? (1UL << 14U) : (1UL << (14U + 16U));
    nibble(value >> 4U);
    nibble(value & 15U);
    delay_us(50U);
}

void lcd_init(void)
{
    RCC_AHBENR_REG |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN;
    (void)RCC_AHBENR_REG;

    /* Set E/RS/data low before switching the pins to outputs. */
    GPIOC_BSRR_REG = (1UL << 13U) | ((1UL << 14U | 1UL << 15U) << 16U);
    GPIOB_BSRR_REG = ((1UL << 8U) | (1UL << 9U)) << 16U;
    GPIOA_BSRR_REG = ((1UL << 12U) | (1UL << 15U)) << 16U;
    GPIOC_MODER_REG = (GPIOC_MODER_REG & ~((3UL << 26U) | (3UL << 28U) | (3UL << 30U))) |
                     (1UL << 26U) | (1UL << 28U) | (1UL << 30U);
    GPIOB_MODER_REG = (GPIOB_MODER_REG & ~((3UL << 16U) | (3UL << 18U))) |
                     (1UL << 16U) | (1UL << 18U);
    GPIOA_MODER_REG = (GPIOA_MODER_REG & ~((3UL << 24U) | (3UL << 30U))) |
                     (1UL << 24U) | (1UL << 30U);

    /* Explicit reset sequence also recovers a powered LCD after MCU-only reset.
     * The first three nibbles establish 8-bit mode before entering 4-bit mode. */
    delay_us(50000U);
    nibble(3U); delay_us(5000U);
    nibble(3U); delay_us(200U);
    nibble(3U); delay_us(200U);
    nibble(2U); delay_us(200U);
    write_byte(0x28U, 0U); /* 4-bit, two lines, 5x8 font */
    write_byte(0x08U, 0U); /* Display off while initialising */
    write_byte(0x01U, 0U); /* Clear requires a longer execution delay */
    delay_us(2000U);
    write_byte(0x06U, 0U); /* Increment cursor, no display shift */
    write_byte(0x0CU, 0U); /* Display on; cursor and blink off */
    display_valid = 0U;
}

void lcd_show_game(const game_state_t *game)
{
    char rows[2][17];
    lcd_format_game(game, rows);
    if (display_valid && memcmp(rows, displayed, sizeof(rows)) == 0) return;

    for (uint8_t row = 0U; row < 2U; row++)
    {
        write_byte(row == 0U ? 0x80U : 0xC0U, 0U);
        for (uint8_t col = 0U; col < 16U; col++) write_byte((uint8_t)rows[row][col], 1U);
    }
    memcpy(displayed, rows, sizeof(displayed));
    display_valid = 1U;
}
