# esphome_sd_card

SD MMC cards components for esphome, it use the SD_MMC library and share its limitations, for more detail see :
[SD_MMC](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC)

# Config

```yaml
sd_mmc_card:
  id: esp_camera_sd_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
```

* **mode_1bit** (Optional, bool): specify wether to use 1 or 4 bit lane
* **clk_pin** : (Optional, GPIO, default=GPIO14): clock pin
* **cmd_pin** : (Optional, GPIO, default=GPIO15): command pin
* **data0_pin**: (Optional, GPIO, default=GPIO2): data 0 pin
* **data1_pin**: (Optional, GPIO): data 1 pin, only use in 4bit mode
* **data2_pin**: (Optional, GPIO): data 2 pin, only use in 4bit mode
* **data3_pin**: (Optional, GPIO): data 3 pin, only use in 4bit mode

In case of connecting in 1-bit lane also known as SPI mode you can use table below to "convert" pin naming:

|SPI naming|MMC naming|
|--|--|
|MISO|DATA0|
|CLK/SCK|CLK|
|MOSI|CMD|
|SS/CS|DATA3|

## Actions

### Write file

```yaml
sd_mmc_card.write_file:
    path: !lambda "/test.txt" 
    data: !lambda |
        std::string str("content");
        return std::vector<uint8_t>(str.begin(), str.end())
```

write a file to the sd card

* **path** (Templatable, string): absolute path to the path
* **data** (Templatable, vector<uint8_t>): file content

### Append file

```yaml
sd_mmc_card.append_file:
    path: "/test.txt" 
    data: !lambda |
        std::string str("new content");
        return std::vector<uint8_t>(str.begin(), str.end())
```
Append content to a file on the sd card

* **path** (Templatable, string): absolute path to the path
* **data** (Templatable, vector<uint8_t>): file content

### Delete file

```yaml
sd_mmc_card.delete_file:
    path: "/test.txt"
```

Delete a file from the sd card

* **path** (Templatable, string): absolute path to the path

### Create directory

```yaml
sd_mmc_card.create_directory:
    path: "/test"
```

Create a folder on the sd card

* **path** (Templatable, string): absolute path to the path

### Remove directory

Delete a folder form the sd card

```yaml
sd_mmc_card.remove_directory:
    path: "/test"
```

## Sensors

### Used space

```yaml
sensor:
  - platform: sd_mmc_card
    type: used_space
    name: "SD card used space"
```

Total used space of the sd card in bytes

* All the [sensor](https://esphome.io/components/sensor/) options

### Total space

```yaml
sensor:
  - platform: sd_mmc_card
    type: total_space
    name: "SD card total space"
```

Total capacity of the sd card

* All the [sensor](https://esphome.io/components/sensor/) options

### File size

```yaml
sensor:
  - platform: sd_mmc_card
    type: file_size
    path: "/test.txt"
```

Return the size of the given file

* **path** (Required, string): path to the file
* All the [sensor](https://esphome.io/components/sensor/) options


## Text Sensor

```yaml
text_sensor:
  - platform: sd_mmc_card
    sd_card_type:
      name: "SD card type"
```

sd card type (MMC, SDSC, ...)

* All the [text sensor](https://esphome.io/components/text_sensor/) options

# Helpers


## Convert Bytes

```cpp
enum MemoryUnits : short {
    Byte = 0,
    KiloByte = 1,
    MegaByte = 2,
    GigaByte = 3,
    TeraByte = 4,
    PetaByte = 5
};

long double convertBytes(uint64_t, MemoryUnits);
```

convert a value in byte to an other memory unit

Example:

```yaml
sensor:
    - platform: sd_mmc_card
        type: file_size
        name: "text.txt size"
        unit_of_measurement: Kb
        path: "/test.txt"
        filters:
        - lambda: return sd_mmc_card::convertBytes(x, sd_mmc_card::MemoryUnits::KiloByte);
```
