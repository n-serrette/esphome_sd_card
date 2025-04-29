#pragma once
#include "esphome/core/automation.h"

#include "storage_base.h"

namespace esphome {
namespace storage_base {

template<typename... Ts> class StorageWriteFileAction : public Action<Ts...> {
 public:
  StorageWriteFileAction(StorageBase *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->write_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  StorageBase *parent_;
};

template<typename... Ts> class StorageAppendFileAction : public Action<Ts...> {
 public:
  StorageAppendFileAction(StorageBase *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->append_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  StorageBase *parent_;
};

template<typename... Ts> class StorageCreateDirectoryAction : public Action<Ts...> {
 public:
  StorageCreateDirectoryAction(StorageBase *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->create_directory(path.c_str());
  }

 protected:
  StorageBase *parent_;
};

template<typename... Ts> class StorageRemoveDirectoryAction : public Action<Ts...> {
 public:
  StorageRemoveDirectoryAction(StorageBase *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->remove_directory(path.c_str());
  }

 protected:
  StorageBase *parent_;
};

template<typename... Ts> class StorageDeleteFileAction : public Action<Ts...> {
 public:
  StorageDeleteFileAction(StorageBase *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->delete_file(path.c_str());
  }

 protected:
  StorageBase *parent_;
};

}  // namespace storage_base
}  // namespace esphome