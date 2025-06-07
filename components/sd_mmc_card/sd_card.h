#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#ifdef USE_ESP_IDF
#include <stdio.h>
#endif

namespace esphome {
namespace sd_mmc_card {

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

#ifdef USE_ESP_IDF
class File {
 public:
  File(FILE *file, size_t size) : file_(file), size_(size) {}
  operator bool() const { return file_.operator bool(); }
  size_t read(char *buff, size_t len) { return fread(buff, 1, len, this->file_.get()); }
  size_t size() const { return this->size_; }
  int seek(size_t off) { return fseek(this->file_.get(), off, SEEK_SET); }
  int fd() const { return fileno(this->file_.get()); }
  operator FILE *() { return file_.get(); }

 protected:
  struct FileDeleter {
    void operator()(FILE *file) { fclose(file); }
  };
  std::unique_ptr<FILE, FileDeleter> file_;
  size_t size_;
};
#else   // USE_ESP_IDF
using File = ::File;
#endif  // USE_ESP_IDF

class SdCard {
 public:
  // object API
  virtual File open(const char *path, const char *mode) = 0;

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