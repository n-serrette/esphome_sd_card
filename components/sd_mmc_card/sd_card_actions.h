#pragma once

#include "sd_card.h"

#include "esphome/core/automation.h"

namespace esphome
{
    namespace sd_mmc_card
    {
        
template<typename... Ts> class SdMmcWriteFileAction : public Action<Ts...> {
    public:
     SdMmcWriteFileAction(SdCard *parent) : parent_(parent) {}
     TEMPLATABLE_VALUE(std::string, path)
     TEMPLATABLE_VALUE(std::vector<uint8_t>, data)
   
     void play(Ts... x) {
       auto path = this->path_.value(x...);
       auto buffer = this->data_.value(x...);
       this->parent_->write_file(path.c_str(), buffer.data(), buffer.size());
     }
   
    protected:
     SdCard *parent_;
   };
   
   template<typename... Ts> class SdMmcAppendFileAction : public Action<Ts...> {
    public:
     SdMmcAppendFileAction(SdCard *parent) : parent_(parent) {}
     TEMPLATABLE_VALUE(std::string, path)
     TEMPLATABLE_VALUE(std::vector<uint8_t>, data)
   
     void play(Ts... x) {
       auto path = this->path_.value(x...);
       auto buffer = this->data_.value(x...);
       this->parent_->append_file(path.c_str(), buffer.data(), buffer.size());
     }
   
    protected:
     SdCard *parent_;
   };
   
   template<typename... Ts> class SdMmcCreateDirectoryAction : public Action<Ts...> {
    public:
     SdMmcCreateDirectoryAction(SdCard *parent) : parent_(parent) {}
     TEMPLATABLE_VALUE(std::string, path)
   
     void play(Ts... x) {
       auto path = this->path_.value(x...);
       this->parent_->create_directory(path.c_str());
     }
   
    protected:
     SdCard *parent_;
   };
   
   template<typename... Ts> class SdMmcRemoveDirectoryAction : public Action<Ts...> {
    public:
     SdMmcRemoveDirectoryAction(SdCard *parent) : parent_(parent) {}
     TEMPLATABLE_VALUE(std::string, path)
   
     void play(Ts... x) {
       auto path = this->path_.value(x...);
       this->parent_->remove_directory(path.c_str());
     }
   
    protected:
     SdCard *parent_;
   };
   
   template<typename... Ts> class SdMmcDeleteFileAction : public Action<Ts...> {
    public:
     SdMmcDeleteFileAction(SdCard *parent) : parent_(parent) {}
     TEMPLATABLE_VALUE(std::string, path)
   
     void play(Ts... x) {
       auto path = this->path_.value(x...);
       this->parent_->delete_file(path.c_str());
     }
   
    protected:
     SdCard *parent_;
   };
    } // namespace sd_mmc_card
    
    
} // namespace esphome
