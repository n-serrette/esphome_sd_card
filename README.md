# esphome_sd_card

A collection of ESPHome components focus on operating a SD card.

To use any of these components in _your_ ESPHome device, check out the documentation for adding [external components](https://esphome.io/components/external_components#git).

> &#x1F6A8; BREAKING CHANGES
> 
> The new `storage_base` component introduce some breaking changes, mainly:
>
>* Actions are now under the `storage` prefix (ex `storage.append_file`)
>* Sensors and text sensors are under the `storage_base` plateform
>* On sd_file_server `sd_mmc_card_id` is remplaced by `storage_id`

## Components

### [storage_base](components/storage_base/README.md)

Base component use to implement a storage device, give a basic interface to interact and manipulate storage. It also give access to device size, file size and storage type sensors.

### [sd_mmc_card](components/sd_mmc_card/README.md) 

Allow reading and writing to the SD card. It also provide some sensors and other utilities to manipulate the card.

basic configuration:
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

### [sd_file_server](components/sd_file_server/README.md)

A simple web page to navigate a storage device content and upload/download/delete files.

basic configuration:
```yaml
sd_file_server:
  id: file_server
  url_prefix: file
  root_path: "/"
  enable_deletion: true
  enable_download: true
  enable_upload: true
```

### Notes

SD MMC is only supported by ESP32 and ESP32-S3 board.

#### Arduino Framework

[sd_mmc_card](components/sd_mmc_card/README.md) does not work entierly with arduino framework version prior to ```2.0.7```.
The issue as been fix by the pull request [espressif/arduino-esp32/#7646](https://github.com/espressif/arduino-esp32/pull/7646)

The recommended version by esphome is ```2.0.5```

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
    version: latest
```

#### ESP-IDF Framework

By default long file name are not enabled, to change this behaviour ```CONFIG_FATFS_LFN_STACK``` or ```CONFIG_FATFS_LFN_HEAP``` should be set in the framework configuration. See the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#config-fatfs-long-filenames) for more detail.

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"
```

## Contributors
[<img src="https://github.com/elproko.png" width="30px;" style="border-radius: 50%;" title="elproko"/>](https://github.com/elproko)
[<img src="https://github.com/youkorr.png" width="30px;" style="border-radius: 50%;" title="youkoor"/>](https://github.com/youkorr)