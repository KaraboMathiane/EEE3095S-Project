#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

#define RCC_AHBENR_REG (*(volatile uint32_t *)0x40021014UL)
#define GPIOB_MODER_REG (*(volatile uint32_t *)0x48000400UL)
#define GPIOB_BSRR_REG (*(volatile uint32_t *)0x48000418UL)

#define RCC_AHBENR_GPIOBEN (1UL << 18)

#define LED_BYTE_MASK 0x00FFUL
#define LED_RED_PIN 10U
#define LED_GREEN_PIN 11U
#define ALL_LED_MASK \
    (LED_BYTE_MASK | (1UL << LED_RED_PIN) | (1UL << LED_GREEN_PIN))

#endif
