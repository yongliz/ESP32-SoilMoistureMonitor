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

void test_median_odd_count(void) {
    uint16_t s[] = {5, 1, 3};
    TEST_ASSERT_EQUAL_UINT16(3, sensor::medianFilter(s, 3));
}

void test_median_even_count(void) {
    uint16_t s[] = {10, 20, 30, 40};
    TEST_ASSERT_EQUAL_UINT16(25, sensor::medianFilter(s, 4));
}

void test_median_rejects_outlier(void) {
    // 一个 4095 尖峰不应影响中值；算术平均会被拉到 ~981，中值仍为 205
    uint16_t s[] = {200, 205, 210, 4095, 198};
    TEST_ASSERT_EQUAL_UINT16(205, sensor::medianFilter(s, 5));
}

void test_median_zero_count(void) {
    uint16_t s[] = {0};
    TEST_ASSERT_EQUAL_UINT16(0, sensor::medianFilter(s, 0));
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
    RUN_TEST(test_median_odd_count);
    RUN_TEST(test_median_even_count);
    RUN_TEST(test_median_rejects_outlier);
    RUN_TEST(test_median_zero_count);
    return UNITY_END();
}
