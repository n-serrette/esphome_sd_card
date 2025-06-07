#include "memory_units.h"

#include <math.h>

namespace esphome {
namespace sd_mmc_card {

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

std::string memory_unit_to_string(MemoryUnits unit) {
  switch (unit) {
    case MemoryUnits::Byte:
      return "B";
    case MemoryUnits::KiloByte:
      return "KB";
    case MemoryUnits::MegaByte:
      return "MB";
    case MemoryUnits::GigaByte:
      return "GB";
    case MemoryUnits::TeraByte:
      return "TB";
    case MemoryUnits::PetaByte:
      return "PB";
  }
  return "unknown";
}

MemoryUnits memory_unit_from_size(size_t size) {
  short unit = MemoryUnits::Byte;
  double s = static_cast<double>(size);
  while (s >= 1024 && unit < MemoryUnits::PetaByte) {
    s /= 1024;
    unit++;
  }
  return static_cast<MemoryUnits>(unit);
}

std::string format_size(size_t size) {
  MemoryUnits unit = memory_unit_from_size(size);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2Lf %s", convertBytes(size, unit), memory_unit_to_string(unit).c_str());
  return std::string(buffer);
}

}  // namespace sd_mmc_card
}  // namespace esphome