#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wio_memo {

constexpr uint8_t kMaxTasks = 12;
constexpr size_t kTaskTitleBytes = 49;
constexpr uint32_t kMinimumValidEpoch = 1577836800UL;  // 2020-01-01 UTC

enum class TaskKind : uint8_t { Todo = 0, Meeting = 1 };
enum class TaskStatus : uint8_t { Pending = 0, Completed = 1, Cancelled = 2 };

enum AlertFlag : uint8_t {
  AdvanceAlertFired = 1 << 0,
  StartAlertFired = 1 << 1,
};

struct Task {
  uint32_t id = 0;
  char title[kTaskTitleBytes]{};
  uint32_t startAtUtc = 0;
  uint32_t endAtUtc = 0;
  uint32_t updatedAtUtc = 0;
  uint32_t snoozedUntilUtc = 0;
  uint16_t remindBeforeMinutes = 0;
  TaskKind kind = TaskKind::Todo;
  TaskStatus status = TaskStatus::Pending;
  uint8_t alertFlags = 0;
  uint8_t reserved = 0;
};

struct TaskList {
  uint8_t count = 0;
  uint8_t reserved[3]{};
  Task items[kMaxTasks]{};
};

inline bool hasAlertFlag(const Task &task, AlertFlag flag) {
  return (task.alertFlags & static_cast<uint8_t>(flag)) != 0;
}

inline void setAlertFlag(Task &task, AlertFlag flag) {
  task.alertFlags |= static_cast<uint8_t>(flag);
}

inline bool isPending(const Task &task) { return task.status == TaskStatus::Pending; }

}  // namespace wio_memo
