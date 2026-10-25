#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

#define RCC_AHBENR_REG (*(volatile uint32_t *)0x40021014UL)
#define RCC_APB2ENR_REG (*(volatile uint32_t *)0x40021018UL)
#define GPIOA_MODER_REG (*(volatile uint32_t *)0x48000000UL)
#define GPIOA_PUPDR_REG (*(volatile uint32_t *)0x4800000CUL)
#define GPIOA_IDR_REG (*(volatile uint32_t *)0x48000010UL)
#define GPIOA_AFRH_REG (*(volatile uint32_t *)0x48000024UL)
#define GPIOB_MODER_REG (*(volatile uint32_t *)0x48000400UL)
#define GPIOB_BSRR_REG (*(volatile uint32_t *)0x48000418UL)
/* STM32F051 USART layout: status, receive data and transmit data are
 * separate registers. Older STM32 SR/DR offsets do not apply here. */
#define USART1_CR1_REG (*(volatile uint32_t *)0x40013800UL)
#define USART1_BRR_REG (*(volatile uint32_t *)0x4001380CUL)
#define USART1_ISR_REG (*(volatile uint32_t *)0x4001381CUL)
#define USART1_ICR_REG (*(volatile uint32_t *)0x40013820UL)
#define USART1_RDR_REG (*(volatile uint32_t *)0x40013824UL)
#define USART1_TDR_REG (*(volatile uint32_t *)0x40013828UL)
#define SYSTICK_CTRL_REG (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD_REG (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL_REG (*(volatile uint32_t *)0xE000E018UL)

#define RCC_AHBENR_GPIOAEN (1UL << 17)
#define RCC_AHBENR_GPIOBEN (1UL << 18)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define USART_ISR_RXNE (1UL << 5)
#define USART_ISR_TXE (1UL << 7)
#define USART_ISR_ERROR_MASK 0x0FUL
#define USART_CR1_RE (1UL << 2)
#define USART_CR1_TE (1UL << 3)
#define USART_CR1_UE (1UL << 0)
#define SYSTICK_CTRL_ENABLE (1UL << 0)
#define SYSTICK_CTRL_CLKSOURCE (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG (1UL << 16)

#define LED_BYTE_MASK 0x00FFUL
#define USER_BUTTON_PIN 0U
#define LED_RED_PIN 10U
#define LED_GREEN_PIN 11U
#define UART_TX_PIN 9U
#define UART_RX_PIN 10U
#define UART_ALTERNATE_FUNCTION 1U
#define UART_BAUD_RATE 115200UL
#define HSI_CLOCK_HZ 8000000UL
#define UART_INTERBYTE_TIMEOUT_MS 1000U
#define STATUS_LED_MASK \
    ((1UL << LED_RED_PIN) | (1UL << LED_GREEN_PIN))
#define USER_BUTTON_MASK (1UL << USER_BUTTON_PIN)

#endif
