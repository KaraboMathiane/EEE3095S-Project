#include "board_config.h"
#include <stdint.h>

int main(void)
{
    static const uint8_t led_pins[] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, LED_RED_PIN, LED_GREEN_PIN
    };
    uint32_t moder;

    RCC_AHBENR_REG |= RCC_AHBENR_GPIOBEN;
    (void)RCC_AHBENR_REG;

    moder = GPIOB_MODER_REG;
    for (uint32_t i = 0U; i < sizeof(led_pins) / sizeof(led_pins[0]); i++)
    {
        const uint32_t shift = 2UL * led_pins[i];
        moder = (moder & ~(3UL << shift)) | (1UL << shift);
    }
    GPIOB_MODER_REG = moder;

    GPIOB_BSRR_REG = ALL_LED_MASK;

    for (;;)
    {
    }
}
