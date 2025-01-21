# esphome_sd_card

A collection of ESPHome components focus on operating a SD card.

To use any of these components in _your_ ESPHome device, check out the documentation for adding [external components](https://esphome.io/components/external_components#git).

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