#pragma once

#include <array>
#include <cstddef>
#include <list>

#include <fcntl.h>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_card.h"
#include "../sd_mmc_card/memory_units.h"

namespace esphome {
namespace sd_file_server {

constexpr size_t max_read = 1024;
constexpr size_t max_send = 1024;
constexpr size_t download_buffer = max_read * 4;
constexpr bool add_noblock_file = true;
constexpr bool add_noblock_response = true;

class SDFileServer : public Component, public AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *);
  void setup() override;
  void dump_config() override;
  void loop() override;

  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool isRequestHandlerTrivial() override { return false; }

  void set_url_prefix(std::string const &);
  void set_root_path(std::string const &);
  void set_sd_mmc_card(sd_mmc_card::SdCard *);
  void set_deletion_enabled(bool);
  void set_download_enabled(bool);
  void set_upload_enabled(bool);

 protected:
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdCard *sd_mmc_card_;

#ifdef USE_ESP_IDF
  struct DownloadResponse {
    class CyclicBuffer {
      std::array<char, download_buffer> buffer_;
      char *write_ptr_ = buffer_.data();
      char *read_ptr_ = buffer_.data();

     public:
      CyclicBuffer() = default;

      CyclicBuffer(const CyclicBuffer &) = delete;
      CyclicBuffer &operator=(const CyclicBuffer &) = delete;

      CyclicBuffer(CyclicBuffer &&) = delete;
      CyclicBuffer &operator=(CyclicBuffer &&) = delete;

      char *read_ptr() { return read_ptr_; }
      char *write_ptr() { return write_ptr_; }
      size_t bytes_to_read() const {
        if (read_ptr_ <= write_ptr_)
          return write_ptr_ - read_ptr_;
        return buffer_.data() + buffer_.size() - read_ptr_;
      }
      size_t bytes_to_write() const {
        if (read_ptr_ > write_ptr_) {
          return read_ptr_ - write_ptr_ - 1;
        }
        if (read_ptr_ == buffer_.data()) {
          return buffer_.data() + buffer_.size() - write_ptr_ - 1;
        }
        return buffer_.data() + buffer_.size() - write_ptr_;
      }
      void submit_write(size_t c) {
        write_ptr_ += c;
        if (write_ptr_ >= buffer_.data() + buffer_.size()) {
          write_ptr_ -= buffer_.size();
        }
      }
      void submit_read(size_t c) {
        read_ptr_ += c;
        if (read_ptr_ >= buffer_.data() + buffer_.size()) {
          read_ptr_ -= buffer_.size();
        }
      }
      bool empty() const { return read_ptr_ == write_ptr_; }
      bool full() const { return bytes_to_write() == 0; }
    } buffer;

    DownloadResponse(sd_mmc_card::File file, httpd_req_t *req, size_t len)
        : file_(std::move(file)), req_(req), bytes_to_send(len) {
      file_fd_ = file_.fd();
      resp_fd_ = httpd_req_to_sockfd(req);
    }

    int file_fd() const { return file_fd_; }
    int resp_fd() const { return resp_fd_; }
    httpd_req_t *req() const { return req_; }
    sd_mmc_card::File &file() { return file_; }

   protected:
    sd_mmc_card::File file_;
    httpd_req_t *req_;
    int file_fd_;
    int resp_fd_;

   public:
    int bytes_sent = 0;
    int bytes_to_send = 0;
    bool scheduled = false;
    bool read_done = false;
    bool completed = false;
    bool failed = false;
  };
  mutable std::list<DownloadResponse> downloadResponses_;
  mutable Mutex downloadResponses_mutex_;
#endif  // USE_ESP_IDF

  std::string url_prefix_;
  std::string root_path_;
  bool deletion_enabled_;
  bool download_enabled_;
  bool upload_enabled_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &) const;
  std::string build_absolute_path(std::string) const;
  void write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const;
  void handle_index(AsyncWebServerRequest *, std::string const &) const;
  void handle_get(AsyncWebServerRequest *) const;
  void handle_delete(AsyncWebServerRequest *);
  void handle_download(AsyncWebServerRequest *, std::string const &) const;
};

}  // namespace sd_file_server
}  // namespace esphome