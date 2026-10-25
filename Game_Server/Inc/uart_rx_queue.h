#ifndef UART_RX_QUEUE_H
#define UART_RX_QUEUE_H

#include <stdint.h>

#define UART_RX_QUEUE_SIZE 128U
#define UART_RX_BAD_BYTE 0x100U

/* Single producer (USART IRQ), single consumer (main loop). Volatile accesses
 * publish each entry before its head index. One slot stays unused. */
typedef struct
{
    volatile uint16_t entries[UART_RX_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t dropped;
} uart_rx_queue_t;

void uart_rx_queue_init(uart_rx_queue_t *queue);
void uart_rx_queue_push(uart_rx_queue_t *queue, uint16_t entry);
int uart_rx_queue_pop(uart_rx_queue_t *queue, uint16_t *entry);

#endif
