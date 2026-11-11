/*
 * Plausibility checks for a touch calibration (NebulaOS, 2026-11-11).
 *
 * The wizard saves its result without a confirmation step, so a stray tap
 * (for example on a popup drawn over the wizard) used to be saved as the
 * calibration - and with touch misaligned there was no way to redo it.
 * These checks reject such a result before it is saved. Pure C, no LVGL,
 * so they are tested offline (tests/test_touch_calibration_check.cpp).
 */
#ifndef LV_TC_CHECK_H
#define LV_TC_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
} lv_tc_check_point_t;

/* The taps on one crosshair lie within `limit` pixels of each other on both
 * axes (min/max are the per-axis extremes of those taps). */
bool lv_tc_check_taps_close(lv_tc_check_point_t min, lv_tc_check_point_t max, int32_t limit);

/* The three touched points span a triangle whose area is between min_pct and
 * max_pct percent of the triangle of the three crosshairs. A tap in the wrong
 * place squashes or stretches it; three taps in one place collapse it. */
bool lv_tc_check_triangle(const lv_tc_check_point_t scr[3], const lv_tc_check_point_t tch[3],
                          int32_t min_pct, int32_t max_pct);

#ifdef __cplusplus
}
#endif

#endif /* LV_TC_CHECK_H */
