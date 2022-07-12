# irnas-lpuart-driver

This repository contains driver for lp uart.

## Setup

Before using the driver, you will need to install nrF SDK. Driver is compatible with NCS 2.0.

If you already have a NCS setup you can follow these steps:

1. To get the driver you need to update `<path to ncs>/ncs/nrf/west.yml`. First in the `remotes` section add:

   ```yaml
    - name: irnas
      url-base: https://github.com/irnas
   ```

2. Then in the `projects` section add at the bottom:

    ```yaml
    - name: irnas-lpuart-driver
      repo-path: irnas-lpuart-driver
      path: irnas/irnas-lpuart-driver
      remote: irnas
      revision: master
    ```

3. Then run `west update` in your freshly created bash/command prompt session.
4. Above command will clone `irnas-lpuart-driver` repository inside of `ncs/irnas/`. You can now use driver code in your application projects.
5. To use driver in your project, add DTS entry .dts or overlay file. For example:

```dts
&uart1 {
    status = "okay";
    compatible = "nordic,nrf-uarte";
    current-speed = <115200>;
    pinctrl-0 = <&uart1_default>;
    pinctrl-1 = <&uart1_sleep>;
    pinctrl-names = "default", "sleep";

    lpuart {
        compatible = "irnas,lpuart";
        status = "okay";
        label = "LPUART";
        signal-gpios = <&gpio0 15 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>;
    };

};

```