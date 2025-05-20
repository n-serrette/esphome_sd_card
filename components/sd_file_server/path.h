#pragma once

#include <string>
#include <vector>

namespace esphome {
namespace sd_file_server {

struct Path {
  static constexpr char separator = '/';

  /* Return the name of the file */
  static std::string file_name(std::string const &);

  /* Is the path an absolute path? */
  static bool is_absolute(std::string const &);

  /* Does the path have a trailing slash? */
  static bool trailing_slash(std::string const &);

  /* Join two path */
  static std::string join(std::string const &, std::string const &);

  static std::string remove_root_path(std::string path, std::string const &root);

  static std::vector<std::string> split_path(std::string path);

  static std::string extension(std::string const &);

  static std::string file_type(std::string const &);

  static std::string mime_type(std::string const &);
};

}  // namespace sd_file_server
}  // namespace esphome