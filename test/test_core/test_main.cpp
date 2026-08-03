#include <string.h>
#include <unity.h>

#include "wio_memo/application/reminder_engine.h"
#include "wio_memo/application/task_service.h"
#include "wio_memo/application/clock.h"
#include "wio_memo/infrastructure/crc32.h"

using namespace wio_memo;

void setUp(void) {}
void tearDown(void) {}

namespace {

Task makeTask(uint32_t id, uint32_t startAt, uint16_t remindBefore = 0) {
  Task task{};
  task.id = id;
  strcpy(task.title, "Design review");
  task.startAtUtc = startAt;
  task.remindBeforeMinutes = remindBefore;
  return task;
}

void test_crc32_known_vector() {
  const char *input = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL, crc32(input, strlen(input)));
}

void test_pending_tasks_are_sorted() {
  TaskList list{};
  list.count = 3;
  list.items[0] = makeTask(1, 3000000000UL);
  list.items[1] = makeTask(2, 2000000000UL);
  list.items[2] = makeTask(3, 2500000000UL);
  list.items[2].status = TaskStatus::Completed;
  TaskService service(list);
  uint8_t indices[kMaxTasks]{};
  TEST_ASSERT_EQUAL_UINT8(2, service.sortedPendingIndices(indices, kMaxTasks));
  TEST_ASSERT_EQUAL_UINT8(1, indices[0]);
  TEST_ASSERT_EQUAL_UINT8(0, indices[1]);
}

void test_replace_preserves_alert_state_only_when_time_matches() {
  TaskList list{};
  list.count = 1;
  list.items[0] = makeTask(9, 2000001000UL, 10);
  list.items[0].alertFlags = AdvanceAlertFired;
  TaskService service(list);
  Task same = makeTask(9, 2000001000UL, 5);
  TEST_ASSERT_EQUAL(TaskResult::Ok, service.replaceAll(&same, 1));
  TEST_ASSERT_TRUE(hasAlertFlag(list.items[0], AdvanceAlertFired));
  Task moved = makeTask(9, 2000002000UL, 5);
  TEST_ASSERT_EQUAL(TaskResult::Ok, service.replaceAll(&moved, 1));
  TEST_ASSERT_FALSE(hasAlertFlag(list.items[0], AdvanceAlertFired));
}

void test_advance_and_start_fire_once() {
  TaskList list{};
  list.count = 1;
  list.items[0] = makeTask(1, 2000000600UL, 10);
  ReminderEngine engine;
  ReminderEvent advance = engine.poll(2000000000UL, list);
  TEST_ASSERT_EQUAL(AlertType::Advance, advance.type);
  TEST_ASSERT_EQUAL(AlertType::None, engine.poll(2000000001UL, list).type);
  TEST_ASSERT_EQUAL(AlertType::Start, engine.poll(2000000600UL, list).type);
  TEST_ASSERT_EQUAL(AlertType::None, engine.poll(2000000601UL, list).type);
}

void test_old_events_are_marked_without_replaying() {
  TaskList list{};
  list.count = 1;
  list.items[0] = makeTask(4, 2000000000UL, 10);
  ReminderEngine engine(300);
  ReminderEvent event = engine.poll(2000001000UL, list);
  TEST_ASSERT_EQUAL(AlertType::None, event.type);
  TEST_ASSERT_TRUE(event.stateChanged);
  TEST_ASSERT_TRUE(hasAlertFlag(list.items[0], AdvanceAlertFired));
  TEST_ASSERT_TRUE(hasAlertFlag(list.items[0], StartAlertFired));
}

void test_snooze_fires_once() {
  TaskList list{};
  list.count = 1;
  list.items[0] = makeTask(7, 2000000000UL);
  list.items[0].alertFlags = StartAlertFired;
  list.items[0].snoozedUntilUtc = 2000000300UL;
  ReminderEngine engine;
  TEST_ASSERT_EQUAL(AlertType::Snooze, engine.poll(2000000300UL, list).type);
  TEST_ASSERT_EQUAL(AlertType::None, engine.poll(2000000301UL, list).type);
}

void test_clock_policy_handles_positive_and_negative_offsets() {
  TEST_ASSERT_EQUAL_UINT32(2000028800UL, ClockPolicy::toLocalEpoch(2000000000UL, 480));
  TEST_ASSERT_EQUAL_UINT32(1999978400UL, ClockPolicy::toLocalEpoch(2000000000UL, -360));
  TEST_ASSERT_TRUE(ClockPolicy::intervalElapsed(100U, 0xFFFFFF00U, 300U));
}

}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_known_vector);
  RUN_TEST(test_pending_tasks_are_sorted);
  RUN_TEST(test_replace_preserves_alert_state_only_when_time_matches);
  RUN_TEST(test_advance_and_start_fire_once);
  RUN_TEST(test_old_events_are_marked_without_replaying);
  RUN_TEST(test_snooze_fires_once);
  RUN_TEST(test_clock_policy_handles_positive_and_negative_offsets);
  return UNITY_END();
}
