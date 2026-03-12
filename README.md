# esphome_sd_card

A collection of ESPHome components focus on operating a SD card.

To use any of these components in _your_ ESPHome device, check out the documentation for adding [external components](https://esphome.io/components/external_components#git).

Minimum ESPHome version is ```2025.7.0```.

## Components

### [sd_mmc_card](components/sd_mmc_card/README.md) 

The main component, allow reading and writing to the SD card. It also provide some sensors and other utilities to manipulate the card.

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

A simple web page to navigate the card content and upload/download/delete files.

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

#### ESPHome

We shoudld disable VFS and LWIP memory saving options to support operations with files. See the [See ESPHome Advanced Configuration](https://esphome.io/components/esp32/#advanced-configuration) for more details.

#### Arduino Framework

```yml
esp32:
  board: esp32dev
  framework:
    type: arduino
    advanced:
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
```

#### ESP-IDF Framework

ESPHome excludes certain built-in IDF components by default to reduce compile time. We should include ```["fatfs", "spiffs"]``` components. [See Built-in IDF Component Inclusion in ESPHome](https://esphome.io/components/esp32/#advanced-configuration)

By default long file name are not enabled, to change this behaviour ```CONFIG_FATFS_LFN_STACK``` or ```CONFIG_FATFS_LFN_HEAP``` should be set in the framework configuration. See the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#config-fatfs-long-filenames) for more detail.

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    advanced:
      include_builtin_idf_components: ["fatfs", "spiffs"]
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"
```

## Contributors
[<img src="https://github.com/elproko.png" width="30px;" style="border-radius: 50%;" title="elproko"/>](https://github.com/elproko)
[<img src="https://github.com/youkorr.png" width="30px;" style="border-radius: 50%;" title="youkoor"/>](https://github.com/youkorr)
[<img src="https://github.com/Yax.png" width="30px;" style="border-radius: 50%;" title="Yax"/>](https://github.com/Yax)
[<img src="https://github.com/denkorolenko.png" width="30px;" style="border-radius: 50%;" title="denkorolenko"/>](https://github.com/denkorolenko)