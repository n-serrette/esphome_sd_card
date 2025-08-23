# esphome_sd_card

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

### Free space

```yaml
sensor:
  - platform: sd_mmc_card
    type: free_space
    name: "SD card free space"
```

Free capacity of the sd card

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

## Others

### List Directory

```cpp
std::vector<std::string> list_directory(const char *path, uint8_t depth);
std::vector<std::string> list_directory(std::string path, uint8_t depth);
```

* **path** : root directory
* **depth**: max depth 

Example

```yaml
- lambda: |
  for (auto const & file : id(esp_camera_sd_card)->list_directory("/", 1))
    ESP_LOGE("   ", "File: %s\n", file.c_str());
```

### List Directory File Info

```cpp
struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);
std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
```

* **path** : root directory
* **depth**: max depth 

Example

```yaml
- lambda: |
  for (auto const & file : id(sd_mmc_card)->list_directory_file_info("/", 1))
    ESP_LOGE("   ", "File: %s, size: %d\n", file.path.c_str(), file.size);
```

### Is Directory

```cpp
bool is_directory(const char *path);
bool is_directory(std::string const &path);
```

* **path**: directory to test path

Example

```yaml
- lambda: return id(sd_mmc_card)->is_directory("/folder");
```

### File Size

```cpp
size_t file_size(const char *path);
size_t file_size(std::string const &path);
```

* **path**: file path

Example

```yaml
- lambda: return id(sd_mmc_card)->file_size("/file");
```

### Read File

```cpp
std::vector<uint8_t> read_file(char const *path);
std::vector<uint8_t> read_file(std::string const &path);
```

Return the whole file as a vector, trying to read large file will saturate the esp memory

* **path**: file path

Example

```yaml
- lambda: return id(sd_mmc_card)->read_file("/file");
```

## Helpers

### Memory Units

```cpp
enum MemoryUnits : short {
    Byte = 0,
    KiloByte = 1,
    MegaByte = 2,
    GigaByte = 3,
    TeraByte = 4,
    PetaByte = 5
};
```

enum representing the different memory units.

### Convert Bytes

```cpp
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

### memory unit to string

```cpp
std::string memory_unit_to_string(MemoryUnits);
```

Return a string representing the memory unit name (ex: KB)

### memory unit from size

```cpp
MemoryUnits memory_unit_from_size(size_t);
```

Deduce the most apropriate memory unit for the given size.

### format size

```cpp
std::string format_size(size_t);
```

Format the given size (in Bytes) to a human readable size (ex: 17.19 KB)