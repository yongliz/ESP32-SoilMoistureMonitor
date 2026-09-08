#include <unity.h>
#include <sensor_math.h>

void setUp(void) {}
void tearDown(void) {}

void test_map_dry_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1600, 1600, 2800));
}

void test_map_wet_is_100(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::mapMoisture(2800, 1600, 2800));
}

void test_map_midpoint_is_50(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, sensor::mapMoisture(2200, 1600, 2800));
}

void test_map_below_dry_clamps_to_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1400, 1600, 2800));
}

void test_map_above_wet_clamps_to_100(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::mapMoisture(3000, 1600, 2800));
}

void test_map_equal_calibration_no_div_by_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1000, 1600, 1600));
}

void test_clamp_percent(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::clampPercent(-5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::clampPercent(150.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, sensor::clampPercent(42.0f));
}

void test_battery_voltage(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.2f, sensor::adcToBatteryVoltage(2.1f, 2.0f));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_map_dry_is_zero);
    RUN_TEST(test_map_wet_is_100);
    RUN_TEST(test_map_midpoint_is_50);
    RUN_TEST(test_map_below_dry_clamps_to_zero);
    RUN_TEST(test_map_above_wet_clamps_to_100);
    RUN_TEST(test_map_equal_calibration_no_div_by_zero);
    RUN_TEST(test_clamp_percent);
    RUN_TEST(test_battery_voltage);
    return UNITY_END();
}
