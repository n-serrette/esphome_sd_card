#pragma once
#include <vector>

#include "esphome/core/component.h"
namespace esphome {
namespace storage_base {

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

class StorageBase : public Component {
 public:
  virtual void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) = 0;
  virtual void write_file(const char *path, const uint8_t *buffer, size_t len);
  virtual void append_file(const char *path, const uint8_t *buffer, size_t len);
  virtual bool delete_file(const char *path) = 0;
  bool delete_file(std::string const &path);
  virtual bool create_directory(const char *path) = 0;
  bool create_directory(std::string const &path);
  virtual bool remove_directory(const char *path) = 0;
  bool remove_directory(std::string const &path);
  virtual std::vector<uint8_t> read_file(char const *path) = 0;
  std::vector<uint8_t> read_file(std::string const &path);
  virtual bool is_directory(const char *path) = 0;
  bool is_directory(std::string const &path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  std::vector<std::string> list_directory(std::string path, uint8_t depth);
  virtual std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth) = 0;
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
  virtual size_t file_size(const char *path) = 0;
  size_t file_size(std::string const &path);
};

long double convertBytes(uint64_t, MemoryUnits);
std::string memory_unit_to_string(MemoryUnits);
MemoryUnits memory_unit_from_size(size_t);
std::string format_size(size_t);
}  // namespace storage_base
}  // namespace esphome