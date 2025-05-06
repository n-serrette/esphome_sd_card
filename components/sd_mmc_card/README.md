# sd_mmc_card

SD MMC cards components for esphome.

* [Config](#config)
  * [Notes](#notes)
    * [Arduino Framework](#arduino-framework)
    * [ESP-IDF Framework](#esp-idf-framework) 
  * [Devices Examples](#devices-examples)
  * [Actions](#actions)
  * [Sensors](#sensors)
  * [Others](#others)
  * [Helpers](#helpers)


# Config

```yaml
sd_mmc_card:
  id: sd_mmc_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
```

* **mode_1bit** (Optional, bool): specify wether to use 1 or 4 bit lane
* **clk_pin** : (Required, [Pin](https://esphome.io/guides/configuration-types#pin)): clock pin
* **cmd_pin** : (Required, [Pin](https://esphome.io/guides/configuration-types#pin)): command pin
* **data0_pin**: (Required, [Pin](https://esphome.io/guides/configuration-types#pin)): data 0 pin
* **data1_pin**: (Optional, [Pin](https://esphome.io/guides/configuration-types#pin)): data 1 pin, only use in 4bit mode
* **data2_pin**: (Optional, [Pin](https://esphome.io/guides/configuration-types#pin)): data 2 pin, only use in 4bit mode
* **data3_pin**: (Optional, [Pin](https://esphome.io/guides/configuration-types#pin)): data 3 pin, only use in 4bit mode
* **power_ctrl_pin**: (Optional, [Pin Schema](https://esphome.io/guides/configuration-types#config-pin-schema)): control the power to the sd card

In case of connecting in 1-bit lane also known as SPI mode you can use table below to "convert" pin naming:

|SPI naming|MMC naming|
|--|--|
|MISO|DATA0|
|CLK/SCK|CLK|
|MOSI|CMD|
|SS/CS|DATA3|

## Notes

### Board

SD MMC is only supported by ESP32 and ESP32-S3 board.

### Arduino Framework

This component use the SD_MMC library and share its limitations, for more detail see :
[SD_MMC](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC)

4 bit lane does not work with arduino framework version prior to ```2.0.7```, due to an issue in the SD_MMC setPins function.
The issue as been fix by the pull request [espressif/arduino-esp32/#7646](https://github.com/espressif/arduino-esp32/pull/7646)

The recommended version by esphome is ```2.0.5```.

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
    version: latest
```

### ESP-IDF Framework

By default long file name are not enabled, to change this behaviour ```CONFIG_FATFS_LFN_STACK``` or ```CONFIG_FATFS_LFN_HEAP``` should be set in the framework configuration. See the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#config-fatfs-long-filenames) for more detail.

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"
```

## Devices Examples

### ESP-Cam

esp cam configuration:

```yaml
sd_mmc_card:
  id: sd_mmc_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
```

### ESP32-S3-Box-3

The ESP32-S3-Box-3 require the use of `power_ctrl_pin` to power the SD card.

```yaml
sd_mmc_card:
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  power_ctrl_pin: GPIO43
```

## Actions

See [storage_base](../storage_base/README.md#actions)

## Sensors

See [storage_base](../storage_base/README.md#sensors)

## Text Sensor

See [storage_base](../storage_base/README.md#text-sensor)

## Others

See [storage_base](../storage_base/README.md#others)

## Helpers

See [storage_base](../storage_base/README.md#helpers)