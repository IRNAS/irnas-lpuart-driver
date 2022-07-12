
zephyr_include_directories(.)
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_UART_LP uart_lp.c)