#include "board_config.h"
#include "hardware_behavior.h"
#include "lcd.h"
#include "server_protocol.h"
#include "uart_rx_queue.h"

#include <stdint.h>

#define UART_LINE_BUFFER_SIZE (SERVER_MAX_COMMAND_LENGTH + 1U)

static uart_rx_queue_t rx_queue;
static uint32_t observed_rx_drops;

void USART1_IRQHandler(void)
{
    const uint32_t status = USART1_ISR_REG;
    uint16_t entry = 0U;

    if ((status & USART_ISR_RXNE) != 0U)
    {
        entry = (uint16_t)(USART1_RDR_REG & 0xFFU);
    }
    if ((status & USART_ISR_ERROR_MASK) != 0U)
    {
        USART1_ICR_REG = status & USART_ISR_ERROR_MASK;
        entry |= UART_RX_BAD_BYTE;
    }
    if ((status & (USART_ISR_RXNE | USART_ISR_ERROR_MASK)) != 0U)
    {
        uart_rx_queue_push(&rx_queue, entry);
    }
}

typedef enum
{
    UART_LINE_READY = 0,
    UART_LINE_BAD_FRAME
} uart_line_status_t;

static void uart_init(void)
{
    uint32_t moder;
    uint32_t afr;

    RCC_AHBENR_REG |= RCC_AHBENR_GPIOAEN;
    RCC_APB2ENR_REG |= RCC_APB2ENR_USART1EN;
    (void)RCC_AHBENR_REG;
    (void)RCC_APB2ENR_REG;

    moder = GPIOA_MODER_REG;
    moder &= ~((3UL << (2UL * UART_TX_PIN)) |
               (3UL << (2UL * UART_RX_PIN)));
    moder |= (2UL << (2UL * UART_TX_PIN)) |
             (2UL << (2UL * UART_RX_PIN));
    GPIOA_MODER_REG = moder;

    afr = GPIOA_AFRH_REG;
    afr &= ~((15UL << (4UL * (UART_TX_PIN - 8U))) |
             (15UL << (4UL * (UART_RX_PIN - 8U))));
    afr |= (UART_ALTERNATE_FUNCTION << (4UL * (UART_TX_PIN - 8U))) |
           (UART_ALTERNATE_FUNCTION << (4UL * (UART_RX_PIN - 8U)));
    GPIOA_AFRH_REG = afr;

    USART1_CR1_REG = 0U;
    USART1_BRR_REG = (HSI_CLOCK_HZ + (UART_BAUD_RATE / 2UL)) / UART_BAUD_RATE;
    uart_rx_queue_init(&rx_queue);
    observed_rx_drops = 0U;
    NVIC_ISER_REG = NVIC_USART1_ENABLE;
    USART1_CR1_REG = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;

    SYSTICK_LOAD_REG = (HSI_CLOCK_HZ / 1000UL) - 1UL;
    SYSTICK_VAL_REG = 0U;
    SYSTICK_CTRL_REG = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_ENABLE;
}

static void uart_write_text(const char *text)
{
    while (*text != '\0')
    {
        while ((USART1_ISR_REG & USART_ISR_TXE) == 0U)
        {
        }

        USART1_TDR_REG = (uint32_t)(uint8_t)*text;
        text++;
    }

    while ((USART1_ISR_REG & USART_ISR_TXE) == 0U)
    {
    }
    USART1_TDR_REG = (uint32_t)'\n';
}

static void board_io_init(void)
{
    uint32_t moder = GPIOA_MODER_REG;
    uint32_t pupdr = GPIOA_PUPDR_REG;

    moder &= ~(3UL << (2UL * USER_BUTTON_PIN));
    moder &= ~(3UL << (2UL * VIEW_BUTTON_PIN));
    GPIOA_MODER_REG = moder;

    pupdr &= ~(3UL << (2UL * USER_BUTTON_PIN));
    pupdr |= 1UL << (2UL * USER_BUTTON_PIN);
    pupdr &= ~(3UL << (2UL * VIEW_BUTTON_PIN));
    pupdr |= 1UL << (2UL * VIEW_BUTTON_PIN);
    GPIOA_PUPDR_REG = pupdr;

    GPIOB_BSRR_REG = STATUS_LED_MASK | (LED_BYTE_MASK << 16U);
}

static void board_write_leds(uint8_t leds, uint8_t progress)
{
    /* The eight individual LEDs are active-high. Preserve the existing
     * status-LED polarity and touch no LCD pins (PB8/PB9). */
    uint32_t set_mask = STATUS_LED_MASK | (uint32_t)progress;
    uint32_t reset_mask = LED_BYTE_MASK & ~(uint32_t)progress;

    if ((leds & HARDWARE_LED_X) != 0U)
    {
        set_mask &= ~(1UL << LED_RED_PIN);
        reset_mask |= 1UL << LED_RED_PIN;
    }
    if ((leds & HARDWARE_LED_O) != 0U)
    {
        set_mask &= ~(1UL << LED_GREEN_PIN);
        reset_mask |= 1UL << LED_GREEN_PIN;
    }

    GPIOB_BSRR_REG = set_mask | (reset_mask << 16U);
}

static void uart_append_line_char(
    char *line,
    uint32_t *length,
    uint8_t *too_long,
    char character)
{
    if (((uint8_t)character < 0x20U) || ((uint8_t)character > 0x7EU))
    {
        *too_long = 1U;
        return;
    }

    if (*too_long != 0U)
    {
        return;
    }

    if (*length < SERVER_MAX_COMMAND_LENGTH)
    {
        line[*length] = character;
        (*length)++;
    }
    else
    {
        *too_long = 1U;
    }
}

static uart_line_status_t uart_read_line(
    char *line,
    game_state_t *game,
    hardware_behavior_t *behavior)
{
    uint32_t length = 0U;
    uint32_t elapsed_ms = 0U;
    uint8_t started = 0U;
    uint8_t pending_cr = 0U;
    uint8_t too_long = 0U;

    for (;;)
    {
        uint16_t entry;

        if (observed_rx_drops != rx_queue.dropped)
        {
            observed_rx_drops = rx_queue.dropped;
            too_long = 1U;
            if (started == 0U)
            {
                started = 1U;
                SYSTICK_VAL_REG = 0U;
            }
        }

        if (uart_rx_queue_pop(&rx_queue, &entry))
        {
            const char character = (char)(entry & 0xFFU);
            if ((entry & UART_RX_BAD_BYTE) != 0U) too_long = 1U;

            started = 1U;
            elapsed_ms = 0U;
            SYSTICK_VAL_REG = 0U;

            if (character == '\n')
            {
                line[length] = '\0';
                return (too_long == 0U) ? UART_LINE_READY : UART_LINE_BAD_FRAME;
            }

            if (pending_cr != 0U)
            {
                uart_append_line_char(line, &length, &too_long, '\r');
                pending_cr = 0U;
            }

            if (character == '\r')
            {
                pending_cr = 1U;
            }
            else
            {
                uart_append_line_char(line, &length, &too_long, character);
            }
        }
        else if ((SYSTICK_CTRL_REG & SYSTICK_CTRL_COUNTFLAG) != 0U)
        {
            const uint8_t button_pressed =
                ((GPIOA_IDR_REG & USER_BUTTON_MASK) == 0U) ? 1U : 0U;
            const uint8_t leds =
                hardware_behavior_tick(behavior, game, button_pressed);

            board_write_leds(leds, hardware_progress_leds(behavior, game));

            /* View changes are local presentation only; the GUI need not sync. */
            if (hardware_view_button_tick(
                    behavior, (GPIOA_IDR_REG & VIEW_BUTTON_MASK) == 0U))
            {
                lcd_show_game(game, behavior->summary_view);
            }

            if (started != 0U)
            {
                elapsed_ms++;
                if (elapsed_ms >= UART_INTERBYTE_TIMEOUT_MS)
                {
                    line[0] = '\0';
                    return UART_LINE_BAD_FRAME;
                }
            }
        }
    }
}

int main(void)
{
    static const uint8_t led_pins[] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, LED_RED_PIN, LED_GREEN_PIN
    };
    game_state_t game;
    hardware_behavior_t behavior;
    char line[UART_LINE_BUFFER_SIZE];
    char response[SERVER_RESPONSE_CAPACITY];
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

    uart_init();
    board_io_init();
    game_init(&game);
    hardware_behavior_init(&behavior);
    lcd_init();
    lcd_show_game(&game, behavior.summary_view);

    for (;;)
    {
        if (uart_read_line(line, &game, &behavior) != UART_LINE_READY)
        {
            line[0] = '\0';
        }

        if (server_handle_command(&game, line, response, sizeof(response)) != 0)
        {
            /* Finish display writes before replying, so a request/response
             * client cannot send its next command during the LCD update. */
            lcd_show_game(&game, behavior.summary_view);
            uart_write_text(response);
        }
        else
        {
            uart_write_text("NACK:BAD");
        }
    }
}
