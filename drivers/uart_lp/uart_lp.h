#ifndef UART_LP_H
#define UART_LP_H

typedef void (*uart_lp_receive_handler_t)(char *);

/**
 * @brief Initialize low power uart helper module
 *
 * @return int 0 on success, negative error code otherwise
 */
int uart_lp_init(void);

/**
 * @brief Register receive handler
 *
 * @param handler The function to be called when a full message is received
 *
 * The terminating character CONFIG_UART_LP_TERMINATOR is stripped from the
 * string before it is used as a parameter to the handler.
 */
void uart_lp_set_receive_handler(uart_lp_receive_handler_t handler);

/**
 * @brief Send string via uart
 *
 * @param message the message to send. Must be valid null-terminated string
 * buffer
 *
 * @return 0 on success
 * @return -EINVAL if message is NULL
 * @return -ENOMEM if message is to big
 */
int uart_lp_send_data(char *data);

#endif // UART_LP_H
