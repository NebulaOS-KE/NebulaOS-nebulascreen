/* Plausibility checks for a touch calibration - see lv_tc_check.h. */
#include "lv_tc_check.h"

bool lv_tc_check_taps_close(lv_tc_check_point_t min, lv_tc_check_point_t max, int32_t limit) {
    return (max.x - min.x) <= limit && (max.y - min.y) <= limit;
}

static int64_t area2(const lv_tc_check_point_t p[3]) {
    int64_t a = (int64_t)(p[1].x - p[0].x) * (p[2].y - p[0].y)
              - (int64_t)(p[2].x - p[0].x) * (p[1].y - p[0].y);
    return a < 0 ? -a : a;
}

bool lv_tc_check_triangle(const lv_tc_check_point_t scr[3], const lv_tc_check_point_t tch[3],
                          int32_t min_pct, int32_t max_pct) {
    int64_t s = area2(scr);
    int64_t t = area2(tch);
    if(s == 0) return false;
    return t * 100 >= s * min_pct && t * 100 <= s * max_pct;
}
