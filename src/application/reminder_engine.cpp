#include "wio_memo/application/reminder_engine.h"

namespace wio_memo {
namespace {

bool inAlertWindow(uint32_t nowUtc, uint32_t eventUtc, uint32_t graceSeconds) {
  return nowUtc >= eventUtc && nowUtc - eventUtc <= graceSeconds;
}

}  // namespace

ReminderEvent ReminderEngine::poll(uint32_t nowUtc, TaskList &tasks) const {
  ReminderEvent result{};
  if (nowUtc < kMinimumValidEpoch) return result;

  for (uint8_t i = 0; i < tasks.count; ++i) {
    Task &task = tasks.items[i];
    if (!isPending(task)) continue;

    if (task.snoozedUntilUtc != 0 && nowUtc >= task.snoozedUntilUtc) {
      const uint32_t snoozedAt = task.snoozedUntilUtc;
      task.snoozedUntilUtc = 0;
      result.stateChanged = true;
      if (inAlertWindow(nowUtc, snoozedAt, graceSeconds_)) {
        result.type = AlertType::Snooze;
        result.taskId = task.id;
        return result;
      }
    }

    if (task.remindBeforeMinutes > 0 && !hasAlertFlag(task, AdvanceAlertFired)) {
      const uint32_t leadSeconds = static_cast<uint32_t>(task.remindBeforeMinutes) * 60UL;
      const uint32_t reminderAt = task.startAtUtc > leadSeconds ? task.startAtUtc - leadSeconds : 0;
      if (nowUtc >= reminderAt) {
        setAlertFlag(task, AdvanceAlertFired);
        result.stateChanged = true;
        if (nowUtc < task.startAtUtc && inAlertWindow(nowUtc, reminderAt, graceSeconds_)) {
          result.type = AlertType::Advance;
          result.taskId = task.id;
          return result;
        }
      }
    }

    if (!hasAlertFlag(task, StartAlertFired) && nowUtc >= task.startAtUtc) {
      setAlertFlag(task, StartAlertFired);
      result.stateChanged = true;
      if (inAlertWindow(nowUtc, task.startAtUtc, graceSeconds_)) {
        result.type = AlertType::Start;
        result.taskId = task.id;
        return result;
      }
    }
  }
  return result;
}

}  // namespace wio_memo
