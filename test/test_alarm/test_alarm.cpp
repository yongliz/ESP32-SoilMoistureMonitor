#include <unity.h>
#include <alarm.h>

void setUp(void) {}
void tearDown(void) {}

void test_below_threshold_sets_latch(void) {
    bool latched = false;
    bool active = alarmctl::evaluate(20.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_TRUE(latched);
    TEST_ASSERT_TRUE(active);
}

void test_above_threshold_plus_hysteresis_clears_latch(void) {
    bool latched = true;
    bool active = alarmctl::evaluate(45.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

void test_deadband_keeps_latch_when_latched(void) {
    bool latched = true;
    bool active = alarmctl::evaluate(35.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_TRUE(latched);
    TEST_ASSERT_TRUE(active);
}

void test_deadband_keeps_latch_when_clear(void) {
    bool latched = false;
    bool active = alarmctl::evaluate(35.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

void test_exact_threshold_does_not_trigger(void) {
    bool latched = false;
    bool active = alarmctl::evaluate(30.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_below_threshold_sets_latch);
    RUN_TEST(test_above_threshold_plus_hysteresis_clears_latch);
    RUN_TEST(test_deadband_keeps_latch_when_latched);
    RUN_TEST(test_deadband_keeps_latch_when_clear);
    RUN_TEST(test_exact_threshold_does_not_trigger);
    return UNITY_END();
}
