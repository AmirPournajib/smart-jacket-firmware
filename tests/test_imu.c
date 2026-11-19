#include "imu.h"
#include "test.h"

static uint32_t feed(fall_detector_t *f, sj_vec3_t a, uint32_t t, uint32_t ms, int *hits)
{
    for (uint32_t i = 0; i < ms; i += 20u) {
        if (fall_update(f, a, t + i)) {
            (*hits)++;
        }
    }
    return t + ms;
}

static void real_fall_is_detected(void)
{
    fall_detector_t f;
    int hits = 0;
    uint32_t t = 0;
    fall_init(&f);
    t = feed(&f, (sj_vec3_t){0, 0, 1}, t, 1000, &hits);
    t = feed(&f, (sj_vec3_t){0, 0, 0.1f}, t, 300, &hits);    /* free fall */
    t = feed(&f, (sj_vec3_t){2.5f, 0, 2.5f}, t, 20, &hits);  /* impact */
    t = feed(&f, (sj_vec3_t){1, 0, 0}, t, 4000, &hits);      /* lying still */
    CHECK(hits == 1);
    CHECK(f.state == FALL_CONFIRMED);
}

static void jump_without_impact_is_ignored(void)
{
    fall_detector_t f;
    int hits = 0;
    uint32_t t = 0;
    fall_init(&f);
    t = feed(&f, (sj_vec3_t){0, 0, 0.2f}, t, 200, &hits);   /* brief low g */
    t = feed(&f, (sj_vec3_t){0, 0, 1}, t, 5000, &hits);     /* lands softly */
    CHECK(hits == 0);
    CHECK(f.state == FALL_IDLE);
}

static void getting_up_after_impact_is_not_a_fall(void)
{
    fall_detector_t f;
    int hits = 0;
    uint32_t t = 0;
    fall_init(&f);
    t = feed(&f, (sj_vec3_t){0, 0, 0.1f}, t, 300, &hits);
    t = feed(&f, (sj_vec3_t){3, 0, 0}, t, 20, &hits);
    for (int k = 0; k < 200; k++) { /* walking away: |a| swings 0.7..1.3 g */
        const float z = (k % 2) ? 1.3f : 0.7f;
        if (fall_update(&f, (sj_vec3_t){0, 0, z}, t)) {
            hits++;
        }
        t += 20u;
    }
    CHECK(hits == 0);
}

static void window_features(void)
{
    act_window_t w;
    act_window_init(&w);
    int ready = 0;
    for (uint32_t i = 0; i < ACT_WINDOW; i++) {
        ready += act_window_push(&w, (sj_vec3_t){0, 0, 1}) ? 1 : 0;
    }
    CHECK(ready == 1);
    float f[ACT_FEATURES];
    act_window_features(&w, f);
    CHECK_NEAR(f[0], 1.0f, 1e-4f);
    CHECK_NEAR(f[1], 0.0f, 1e-3f);
    CHECK_NEAR(f[3], 0.0f, 1e-6f);
    CHECK_NEAR(f[4], 1.0f, 1e-4f);
    /* After that, a new window every half window (1 s). */
    ready = 0;
    for (uint32_t i = 0; i < ACT_WINDOW; i++) {
        ready += act_window_push(&w, (sj_vec3_t){0, 0, 1}) ? 1 : 0;
    }
    CHECK(ready == 2);
}

void test_imu(void)
{
    real_fall_is_detected();
    jump_without_impact_is_ignored();
    getting_up_after_impact_is_not_a_fall();
    window_features();
}
