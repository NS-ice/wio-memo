#include "wio_memo/application/task_service.h"

#include <string.h>

namespace wio_memo {

TaskResult TaskService::validate(const Task &task) {
  const size_t titleBytes = strnlen(task.title, kTaskTitleBytes);
  if (titleBytes == 0 || titleBytes >= kTaskTitleBytes) return TaskResult::InvalidTitle;
  if (task.startAtUtc < kMinimumValidEpoch ||
      (task.endAtUtc != 0 && task.endAtUtc < task.startAtUtc) ||
      task.remindBeforeMinutes > 1440) {
    return TaskResult::InvalidTime;
  }
  if (task.kind != TaskKind::Todo && task.kind != TaskKind::Meeting) return TaskResult::InvalidKind;
  if (task.status != TaskStatus::Pending && task.status != TaskStatus::Completed &&
      task.status != TaskStatus::Cancelled) {
    return TaskResult::InvalidStatus;
  }
  return TaskResult::Ok;
}

TaskResult TaskService::replaceAll(const Task *incoming, uint8_t count) {
  if (count > kMaxTasks) return TaskResult::TooManyTasks;
  for (uint8_t i = 0; i < count; ++i) {
    const TaskResult result = validate(incoming[i]);
    if (result != TaskResult::Ok) return result;
  }

  TaskList replacement{};
  replacement.count = count;
  for (uint8_t i = 0; i < count; ++i) {
    replacement.items[i] = incoming[i];
    for (uint8_t old = 0; old < tasks_.count; ++old) {
      const Task &previous = tasks_.items[old];
      if (previous.id == incoming[i].id && previous.startAtUtc == incoming[i].startAtUtc) {
        replacement.items[i].alertFlags = previous.alertFlags;
        replacement.items[i].snoozedUntilUtc = previous.snoozedUntilUtc;
        break;
      }
    }
  }
  tasks_ = replacement;
  return TaskResult::Ok;
}

TaskResult TaskService::setStatus(uint32_t id, TaskStatus status, uint32_t updatedAtUtc) {
  if (status != TaskStatus::Pending && status != TaskStatus::Completed &&
      status != TaskStatus::Cancelled) {
    return TaskResult::InvalidStatus;
  }
  Task *task = find(id);
  if (!task) return TaskResult::NotFound;
  task->status = status;
  task->updatedAtUtc = updatedAtUtc;
  return TaskResult::Ok;
}

TaskResult TaskService::snooze(uint32_t id, uint32_t untilUtc, uint32_t updatedAtUtc) {
  Task *task = find(id);
  if (!task) return TaskResult::NotFound;
  if (!isPending(*task) || untilUtc <= updatedAtUtc) return TaskResult::InvalidTime;
  task->snoozedUntilUtc = untilUtc;
  task->updatedAtUtc = updatedAtUtc;
  return TaskResult::Ok;
}

Task *TaskService::find(uint32_t id) {
  for (uint8_t i = 0; i < tasks_.count; ++i) if (tasks_.items[i].id == id) return &tasks_.items[i];
  return nullptr;
}

const Task *TaskService::find(uint32_t id) const {
  for (uint8_t i = 0; i < tasks_.count; ++i) if (tasks_.items[i].id == id) return &tasks_.items[i];
  return nullptr;
}

uint8_t TaskService::sortedPendingIndices(uint8_t *indices, uint8_t capacity) const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < tasks_.count && count < capacity; ++i) {
    if (isPending(tasks_.items[i])) indices[count++] = i;
  }
  for (uint8_t i = 1; i < count; ++i) {
    const uint8_t value = indices[i];
    int j = i - 1;
    while (j >= 0 && tasks_.items[indices[j]].startAtUtc > tasks_.items[value].startAtUtc) {
      indices[j + 1] = indices[j];
      --j;
    }
    indices[j + 1] = value;
  }
  return count;
}

}  // namespace wio_memo
