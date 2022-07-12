#include <uart_lp.h>

#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <pm/device.h>
#include <zephyr.h>

#include <stdio.h>
#include <string.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(lpuart, CONFIG_UART_LP_LOG_LEVEL);

// extract from device tree
#define DT_DRV_COMPAT irnas_lpuart

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(uart)
#define UART_LABEL DT_INST_BUS_LABEL(0)
#define SIGNAL_LABEL DT_INST_GPIO_LABEL(0, signal_gpios)
#define SIGNAL_PIN DT_INST_GPIO_PIN(0, signal_gpios)
#define SIGNAL_FLAGS DT_INST_GPIO_FLAGS(0, signal_gpios)
#else
#error "No lpuart defined on any uart in dts"
#endif

// extract from Kconfig
#define BUF_SIZE (CONFIG_UART_LP_BUF_SIZE + 2)
#define TERMINATOR CONFIG_UART_LP_TERMINATOR

static struct gpio_callback signal_ext_int_cb;

void signal_interrupt_cb_fun(const struct device *dev, struct gpio_callback *cb,
			     gpio_port_pins_t pin);
void signal_interrupt_configure(void);
void signal_interrupt_set_to_input(bool iterrupt_active);
void signal_interrupt_set_to_output(int value);
void uart_turn_on(void);
void uart_turn_off(void);

int uart_send(uint8_t *buffer, uint16_t len);

static const struct device *uart_dev;
static const struct device *gpio_dev;

/* GPIO FUNCTIONS */

// pin interrupt callback
static bool interrupt_line_high = false;
void signal_interrupt_cb_fun(const struct device *dev, struct gpio_callback *cb,
			     gpio_port_pins_t pin)
{
	if (!interrupt_line_high) {
		interrupt_line_high = true;
		LOG_DBG("signal pin going high");

		// set interrupt to line low
		signal_interrupt_set_to_input(0);

		// switch uart on
		uart_turn_on();

		// enable rx to be able to receive
		uart_irq_rx_enable(uart_dev);
	} else {
		interrupt_line_high = false;
		LOG_DBG("signal pin going low");

		// disable rx
		uart_irq_rx_disable(uart_dev);

		// switch uart to log
		uart_turn_off();

		// set interrupt to line high
		signal_interrupt_set_to_input(1);
	}
}

// configure pin for interrupt
void signal_interrupt_configure(void)
{
	gpio_dev = device_get_binding(SIGNAL_LABEL);

	int err = gpio_pin_configure(gpio_dev, SIGNAL_PIN,
				     GPIO_INPUT | SIGNAL_FLAGS);
	if (err) {
		LOG_ERR("gpio_pin_configure err: %d", err);
	}

	err = gpio_pin_interrupt_configure(gpio_dev, SIGNAL_PIN,
					   GPIO_INT_LEVEL_ACTIVE);
	if (err) {
		LOG_ERR("gpio_pin_interrupt_configure err: %d", err);
	}

	gpio_init_callback(&signal_ext_int_cb, signal_interrupt_cb_fun,
			   BIT(SIGNAL_PIN));
	err = gpio_add_callback(gpio_dev, &signal_ext_int_cb);
	if (err) {
		LOG_ERR("gpio_add_callback err: %d", err);
	}
}

// state can be 0 or 1
void signal_interrupt_set_to_input(bool interrupt_active)
{
	int err;

	err = gpio_pin_configure(gpio_dev, SIGNAL_PIN,
				 GPIO_INPUT | SIGNAL_FLAGS);
	if (err) {
		LOG_ERR("gpio_pin_configure err: %d", err);
	}
	if (interrupt_active) {
		err = gpio_pin_interrupt_configure(gpio_dev, SIGNAL_PIN,
						   GPIO_INT_LEVEL_ACTIVE);
		if (err) {
			LOG_ERR("gpio_pin_interrupt_configure err: %d", err);
		}
	} else {
		int err = gpio_pin_interrupt_configure(gpio_dev, SIGNAL_PIN,
						       GPIO_INT_LEVEL_INACTIVE);
		if (err) {
			LOG_ERR("gpio_pin_interrupt_configure err: %d", err);
		}
	}
	err = gpio_add_callback(gpio_dev, &signal_ext_int_cb);
	if (err) {
		LOG_ERR("gpio_add_callback err: %d", err);
	}
}

void signal_interrupt_set_to_output(int value)
{
	int err = gpio_remove_callback(gpio_dev, &signal_ext_int_cb);
	if (err) {
		LOG_ERR("gpio_remove_callback err: %d", err);
	}
	err = gpio_pin_interrupt_configure(gpio_dev, SIGNAL_PIN,
					   GPIO_INT_DISABLE);
	if (err) {
		LOG_ERR("gpio_pin_interrupt_configure err: %d", err);
	}
	err = gpio_pin_configure(gpio_dev, SIGNAL_PIN,
				 GPIO_OUTPUT | SIGNAL_FLAGS);
	if (err) {
		LOG_ERR("gpio_pin_configure err: %d", err);
	}
	gpio_pin_set(gpio_dev, SIGNAL_PIN, value);
}

void uart_turn_on(void)
{
	LOG_DBG("Turning uart on");
	pm_device_action_run(uart_dev, PM_DEVICE_ACTION_RESUME);
}

void uart_turn_off(void)
{
	LOG_DBG("Turning uart off");
	pm_device_action_run(uart_dev, PM_DEVICE_ACTION_SUSPEND);
}

/* INTERRUPT DRIVEN API */

// struct to keep the message when it is put in the fifo queue, where it waits
// to be sent
struct uart_data {
	uint8_t buffer[BUF_SIZE];
	uint16_t len;
};

// serial device.
static struct serial_dev_user_data {
	const struct device *dev;
	struct uart_data *rx;
} s_dev_usr_data;

// the handler function pointer
uart_lp_receive_handler_t _uart_receive_handler = NULL;

void uart_cb(const struct device *dev, void *x)
{
	struct serial_dev_user_data *sd = x;

	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev)) {
		while (!sd->rx) {
			sd->rx = k_malloc(sizeof(*sd->rx));
			if (sd->rx) {
				sd->rx->len = 0;
			}
		}

		int data_length =
			uart_fifo_read(dev, &sd->rx->buffer[sd->rx->len],
				       BUF_SIZE - sd->rx->len);
		sd->rx->len += data_length;
		// LOG_DBG("RECEIVED BUFFER: %s, LEN: %d", sd->rx->buffer,
		// sd->rx->len);

		// trigger callback on new line
		if (sd->rx->buffer[sd->rx->len - 1] == TERMINATOR) {
			// remove whole terminator before sending msg into
			// handler
			sd->rx->buffer[sd->rx->len - 1] = '\0';

			// call attached handler function
			// but only if the string is not just a new line
			// character
			if (_uart_receive_handler != NULL) {
				_uart_receive_handler(sd->rx->buffer);
			} else {
				LOG_WRN("Received data over uart, but no "
					"handler is set: [%s]",
					sd->rx->buffer);
			}

			// reset buffer len (so new msg overwrites old)
			sd->rx->len = 0;
		}
	}

	// TX is done via poll_out, so nothing to be handled here
}

int uart_send(uint8_t *buffer, uint16_t len)
{
	if (buffer == NULL) {
		return -EINVAL;
	}
	if (len > BUF_SIZE - 2) {
		return -ENOMEM;
	}

	// copy to internal buffer
	struct uart_data data = {0};
	data.len = len;
	// copy buffer
	memcpy(data.buffer, buffer, data.len);

	// // add terminating character
	data.buffer[data.len] = TERMINATOR;
	data.len++;

	// if sending while interrupt line is not high,
	// we must set it high ourselves to tell other MCU we are sending
	bool was_enabled = false;
	if (!interrupt_line_high) {
		signal_interrupt_set_to_output(1);
		k_sleep(K_MSEC(1));
		uart_turn_on();
		uart_irq_rx_enable(uart_dev);
		was_enabled = true;
	}

	LOG_DBG("Sending: [%s]", data.buffer);

	// k_fifo_put(s_dev_usr_data.tx_fifo, data);
	// uart_irq_tx_enable(s_dev_usr_data.dev);

	for (int i = 0; i < data.len; i++) {
		uart_poll_out(s_dev_usr_data.dev, data.buffer[i]);
		// LOG_DBG("Poll put: %c", buffer[i]);
	}

	// undo uart setup above after short delay
	if (was_enabled) {
		k_sleep(K_MSEC(1));
		signal_interrupt_set_to_input(1);
		k_sleep(K_MSEC(1));
		uart_irq_rx_disable(uart_dev);
		uart_turn_off();
	}
	return 0;
}

/* EXPOSED API */

int uart_lp_init(void)
{
	LOG_INF("Initializing lpuart controller");

	// configure callback and interrupt driven uart system
	uart_dev = device_get_binding(UART_LABEL);
	if (!uart_dev) {
		LOG_ERR("UART device not found");
		return -ENODEV;
	}

	// register gpio interrupt
	signal_interrupt_configure();
	signal_interrupt_set_to_input(1);
	k_sleep(K_MSEC(100));

	s_dev_usr_data.dev = uart_dev;

	uart_irq_callback_user_data_set(uart_dev, uart_cb, &s_dev_usr_data);

	// disable uart to save power
	if (!interrupt_line_high) {
		uart_turn_off();
	}

	return 0;
}

void uart_lp_set_receive_handler(uart_lp_receive_handler_t handler)
{
	_uart_receive_handler = handler;
}

int uart_lp_send_data(char *data)
{
	if (data == NULL) {
		return -EINVAL;
	}
	return uart_send(data, strlen(data));
}
