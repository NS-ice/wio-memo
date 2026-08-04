#include "wio_memo/infrastructure/persistent_store.h"

#include <FlashStorage_SAMD.h>
#include <stddef.h>
#include <string.h>

#include "wio_memo/infrastructure/crc32.h"

using WioMemoPersistentImage = wio_memo::PersistentImage;
FlashStorage(wioMemoStorage, WioMemoPersistentImage);

namespace wio_memo {
namespace {

struct LegacyStoreSlot {
  uint32_t magic;
  uint16_t schemaVersion;
  uint16_t reserved;
  uint32_t sequence;
  TaskList tasks;
  uint32_t checksum;
};

struct LegacyPersistentImage {
  LegacyStoreSlot slots[2];
};

bool validLegacy(const LegacyStoreSlot &slot) {
  return slot.magic == kStoreMagic && slot.schemaVersion == 2 && slot.tasks.count <= kMaxTasks &&
         slot.checksum == crc32(&slot, offsetof(LegacyStoreSlot, checksum));
}

int newestLegacySlot(const LegacyPersistentImage &image) {
  const bool first = validLegacy(image.slots[0]);
  const bool second = validLegacy(image.slots[1]);
  if (!first && !second) return -1;
  if (!first) return 1;
  if (!second) return 0;
  return static_cast<int32_t>(image.slots[1].sequence - image.slots[0].sequence) > 0 ? 1 : 0;
}

}  // namespace

bool PersistentStore::valid(const StoreSlot &slot) const {
  if (slot.magic != kStoreMagic || slot.schemaVersion != kStoreSchemaVersion ||
      slot.tasks.count > kMaxTasks) {
    return false;
  }
  return slot.checksum == crc32(&slot, offsetof(StoreSlot, checksum));
}

int PersistentStore::newestValidSlot(const PersistentImage &image) const {
  const bool first = valid(image.slots[0]);
  const bool second = valid(image.slots[1]);
  if (!first && !second) return -1;
  if (!first) return 1;
  if (!second) return 0;
  return static_cast<int32_t>(image.slots[1].sequence - image.slots[0].sequence) > 0 ? 1 : 0;
}

bool PersistentStore::begin(TaskList &tasks, DeviceSettings &settings) {
  PersistentImage image{};
  wioMemoStorage.read(image);
  const int slot = newestValidSlot(image);
  if (slot < 0) {
    LegacyPersistentImage legacy{};
    memcpy(&legacy, &image, sizeof(legacy));
    const int legacySlot = newestLegacySlot(legacy);
    if (legacySlot >= 0) {
      tasks = legacy.slots[legacySlot].tasks;
      settings = DeviceSettings{};
      settings_ = settings;
      activeSequence_ = legacy.slots[legacySlot].sequence;
      return write(tasks, settings);
    }
    tasks = TaskList{};
    settings = DeviceSettings{};
    settings_ = settings;
    activeSequence_ = 0;
    return write(tasks, settings);
  }
  tasks = image.slots[slot].tasks;
  settings = image.slots[slot].settings;
  settings_ = settings;
  activeSequence_ = image.slots[slot].sequence;
  return true;
}

bool PersistentStore::save(const TaskList &tasks) {
  return write(tasks, settings_);
}

bool PersistentStore::saveSettings(const TaskList &tasks, const DeviceSettings &settings) {
  if (!write(tasks, settings)) return false;
  settings_ = settings;
  return true;
}

bool PersistentStore::write(const TaskList &tasks, const DeviceSettings &settings) {
  if (tasks.count > kMaxTasks) return false;
  PersistentImage image{};
  wioMemoStorage.read(image);
  const int active = newestValidSlot(image);
  const int target = active == 0 ? 1 : 0;
  StoreSlot replacement{};
  replacement.magic = kStoreMagic;
  replacement.schemaVersion = kStoreSchemaVersion;
  replacement.sequence = activeSequence_ + 1;
  replacement.tasks = tasks;
  replacement.settings = settings;
  replacement.checksum = crc32(&replacement, offsetof(StoreSlot, checksum));
  image.slots[target] = replacement;
  wioMemoStorage.write(image);
  activeSequence_ = replacement.sequence;
  return true;
}

}  // namespace wio_memo
