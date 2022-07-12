
#include <logging/log.h>
#include <uart_lp.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main);

char *messages[] = {
	"hello",
	"aloha",
	"zdravo",
};

static void uart_lp_receive_handler(char *msg)
{
	/* print what was received */
	LOG_INF("RECEIVED: [%s]", msg);
}

void main(void)
{
	int err;

	err = uart_lp_init();
	if (err) {
		LOG_ERR("uart_lp_init, err: %d", err);
		return;
	}
	uart_lp_set_receive_handler(uart_lp_receive_handler);

	/* send a message every so often */
	while (1) {
		for (int i = 0; i < ARRAY_SIZE(messages); i++) {
			LOG_INF("SENDING: [%s]", messages[i]);
			uart_lp_send_data(messages[i]);
			k_sleep(K_SECONDS(1));
		}
	}
}