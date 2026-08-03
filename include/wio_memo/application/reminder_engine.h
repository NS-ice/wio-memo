#pragma once

#include "wio_memo/domain/task.h"

namespace wio_memo {

enum class AlertType : uint8_t { None = 0, Advance = 1, Start = 2, Snooze = 3 };

struct ReminderEvent {
  AlertType type = AlertType::None;
  uint32_t taskId = 0;
  bool stateChanged = false;
};

class ReminderEngine {
 public:
  explicit ReminderEngine(uint32_t graceSeconds = 300) : graceSeconds_(graceSeconds) {}
  ReminderEvent poll(uint32_t nowUtc, TaskList &tasks) const;

 private:
  uint32_t graceSeconds_;
};

}  // namespace wio_memo
