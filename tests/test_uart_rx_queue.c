/* Receive-buffer tests simulate bytes arriving while main performs slow work. */
#include "uart_rx_queue.h"
#include <assert.h>

int main(void)
{
    uart_rx_queue_t queue;
    uint16_t entry;
    uart_rx_queue_init(&queue);
    assert(!uart_rx_queue_pop(&queue, &entry));

    /* Buffer multiple complete commands in FIFO order, including error metadata. */
    const uint16_t bytes[] = {'S','T','A','T','E','\n', UART_RX_BAD_BYTE | 'X'};
    for (unsigned i = 0U; i < sizeof(bytes) / sizeof(bytes[0]); i++)
        uart_rx_queue_push(&queue, bytes[i]);
    for (unsigned i = 0U; i < sizeof(bytes) / sizeof(bytes[0]); i++)
    {
        assert(uart_rx_queue_pop(&queue, &entry));
        assert(entry == bytes[i]);
    }

    /* Repeated bursts exercise head/tail wraparound without byte corruption. */
    for (unsigned round = 0U; round < 20U; round++)
    {
        for (unsigned i = 0U; i < 100U; i++) uart_rx_queue_push(&queue, (uint16_t)i);
        for (unsigned i = 0U; i < 100U; i++)
        {
            assert(uart_rx_queue_pop(&queue, &entry));
            assert(entry == i);
        }
    }
    assert(queue.dropped == 0U);

    /* A full ring preserves unread entries and explicitly records lost data. */
    for (unsigned i = 0U; i < UART_RX_QUEUE_SIZE - 1U; i++)
        uart_rx_queue_push(&queue, (uint16_t)i);
    uart_rx_queue_push(&queue, 999U);
    uart_rx_queue_push(&queue, 998U);
    assert(queue.dropped == 2U);
    for (unsigned i = 0U; i < UART_RX_QUEUE_SIZE - 1U; i++)
    {
        assert(uart_rx_queue_pop(&queue, &entry));
        assert(entry == i);
    }
    assert(!uart_rx_queue_pop(&queue, &entry));
    uart_rx_queue_push(&queue, 'N');
    assert(uart_rx_queue_pop(&queue, &entry) && entry == 'N');
    return 0;
}
