#include "uart_rx_queue.h"

void uart_rx_queue_init(uart_rx_queue_t *queue)
{
    queue->head = queue->tail = queue->dropped = 0U;
}

void uart_rx_queue_push(uart_rx_queue_t *queue, uint16_t entry)
{
    const uint32_t next = (queue->head + 1U) % UART_RX_QUEUE_SIZE;
    if (next == queue->tail)
    {
        /* Never overwrite unread bytes. The main parser rejects the affected
         * frame after observing this monotonically increasing loss counter. */
        queue->dropped++;
        return;
    }
    queue->entries[queue->head] = entry;
    queue->head = next;
}

int uart_rx_queue_pop(uart_rx_queue_t *queue, uint16_t *entry)
{
    if (queue->tail == queue->head) return 0;
    *entry = queue->entries[queue->tail];
    queue->tail = (queue->tail + 1U) % UART_RX_QUEUE_SIZE;
    return 1;
}
