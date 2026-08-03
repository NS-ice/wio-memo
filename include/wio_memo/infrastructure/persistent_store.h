#pragma once

#include <stdint.h>

#include "wio_memo/domain/task.h"

namespace wio_memo {

constexpr uint32_t kStoreMagic = 0x574D5332UL;  // WMS2
constexpr uint16_t kStoreSchemaVersion = 3;

struct DeviceSettings {
  uint8_t networkConfigured = 0;
  uint8_t reserved = 0;
  int16_t utcOffsetMinutes = 480;
  char stationSsid[33]{};
  char stationPassword[65]{};
  char accessPointSsid[33] = "WioMemo-Setup";
  char accessPointPassword[65] = "wio-memo";
};

struct StoreSlot {
  uint32_t magic = 0;
  uint16_t schemaVersion = 0;
  uint16_t reserved = 0;
  uint32_t sequence = 0;
  TaskList tasks{};
  DeviceSettings settings{};
  uint32_t checksum = 0;
};

struct PersistentImage {
  StoreSlot slots[2]{};
};

class PersistentStore {
 public:
  bool begin(TaskList &tasks, DeviceSettings &settings);
  bool save(const TaskList &tasks);
  bool saveSettings(const TaskList &tasks, const DeviceSettings &settings);
  uint32_t activeSequence() const { return activeSequence_; }

 private:
  bool valid(const StoreSlot &slot) const;
  int newestValidSlot(const PersistentImage &image) const;
  bool write(const TaskList &tasks, const DeviceSettings &settings);
  uint32_t activeSequence_ = 0;
  DeviceSettings settings_{};
};

}  // namespace wio_memo
