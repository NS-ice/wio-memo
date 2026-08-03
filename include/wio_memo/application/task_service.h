#pragma once

#include "wio_memo/domain/task.h"

namespace wio_memo {

enum class TaskResult : uint8_t {
  Ok,
  InvalidTitle,
  InvalidTime,
  InvalidKind,
  InvalidStatus,
  TooManyTasks,
  NotFound,
};

class TaskService {
 public:
  explicit TaskService(TaskList &tasks) : tasks_(tasks) {}

  static TaskResult validate(const Task &task);
  TaskResult replaceAll(const Task *incoming, uint8_t count);
  TaskResult setStatus(uint32_t id, TaskStatus status, uint32_t updatedAtUtc);
  TaskResult snooze(uint32_t id, uint32_t untilUtc, uint32_t updatedAtUtc);
  Task *find(uint32_t id);
  const Task *find(uint32_t id) const;
  uint8_t sortedPendingIndices(uint8_t *indices, uint8_t capacity) const;

 private:
  TaskList &tasks_;
};

}  // namespace wio_memo
