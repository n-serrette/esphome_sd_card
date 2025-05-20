#pragma once

#include <string>
#include <vector>

namespace esphome {
namespace sd_mmc_card {

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

class SdCard {
 public:
  // sugar
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);
  bool delete_file(std::string const &path);
  std::vector<uint8_t> read_file(std::string const &path);
  bool is_directory(std::string const &path);
  std::vector<std::string> list_directory(std::string path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
  size_t file_size(std::string const &path);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);

  virtual void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) = 0;
  virtual bool delete_file(const char *path) = 0;
  virtual bool create_directory(const char *path) = 0;
  virtual bool remove_directory(const char *path) = 0;
  virtual std::vector<uint8_t> read_file(char const *path) = 0;
  virtual bool is_directory(const char *path) = 0;
  virtual size_t file_size(const char *path) = 0;

 protected:
  virtual std::vector<FileInfo> &list_directory_file_info_rec(const char *path, uint8_t depth,
                                                              std::vector<FileInfo> &list) = 0;
};
}  // namespace sd_mmc_card
}  // namespace esphome