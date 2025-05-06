# storage_base

Base component use to implement a storage device, give a basic interface to interact and manipulate storage.

* [Actions](#actions)
* [Sensors](#sensors)
* [Others](#others)
* [Helpers](#helpers)

## Actions

### Write file

```yaml
storage.write_file:
    path: !lambda "/test.txt" 
    data: !lambda |
        std::string str("content");
        return std::vector<uint8_t>(str.begin(), str.end())
```

write a file to the storage device

* **path** (Templatable, string): absolute path to the path
* **data** (Templatable, vector<uint8_t>): file content

### Append file

```yaml
storage.append_file:
    path: "/test.txt" 
    data: !lambda |
        std::string str("new content");
        return std::vector<uint8_t>(str.begin(), str.end())
```
Append content to a file on the storage device

* **path** (Templatable, string): absolute path to the path
* **data** (Templatable, vector<uint8_t>): file content

### Delete file

```yaml
storage.delete_file:
    path: "/test.txt"
```

Delete a file from the storage device

* **path** (Templatable, string): absolute path to the path

### Create directory

```yaml
storage.create_directory:
    path: "/test"
```

Create a folder on the storage device

* **path** (Templatable, string): absolute path to the path

### Remove directory

Delete a folder form the storage device

```yaml
storage.remove_directory:
    path: "/test"
```

## Sensors

### Used space

```yaml
sensor:
  - platform: storage_base
    type: used_space
    name: "Storage used space"
```

Total used space of the storage device in bytes

* All the [sensor](https://esphome.io/components/sensor/) options

### Total space

```yaml
sensor:
  - platform: storage_base
    type: total_space
    name: "Storage total space"
```

Total capacity of the storage device

* All the [sensor](https://esphome.io/components/sensor/) options

### Free space

```yaml
sensor:
  - platform: storage_base
    type: free_space
    name: "Storage free space"
```

Free capacity of the storage device

* All the [sensor](https://esphome.io/components/sensor/) options

### File size

```yaml
sensor:
  - platform: storage_base
    type: file_size
    path: "/test.txt"
```

Return the size of the given file

* **path** (Required, string): path to the file
* All the [sensor](https://esphome.io/components/sensor/) options


## Text Sensor

```yaml
text_sensor:
  - platform: storage_base
    storage_type:
      name: "SD card type"
```

storage type (MMC, SDSC, ...)

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
  for (auto const & file : id(storage)->list_directory("/", 1))
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
  for (auto const & file : id(storage)->list_directory_file_info("/", 1))
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
- lambda: return id(storage)->is_directory("/folder");
```

### File Size

```cpp
size_t file_size(const char *path);
size_t file_size(std::string const &path);
```

* **path**: file path

Example

```yaml
- lambda: return id(storage)->file_size("/file");
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
- lambda: return id(storage)->read_file("/file");
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
    - platform: storage_base
        type: file_size
        name: "text.txt size"
        unit_of_measurement: Kb
        path: "/test.txt"
        filters:
        - lambda: return storage_base::convertBytes(x, storage_base::MemoryUnits::KiloByte);
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