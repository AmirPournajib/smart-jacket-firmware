/* Motion: fall detection state machine and activity feature window. */
#ifndef IMU_H
#define IMU_H

#include "sj_types.h"

/* ---- Fall detection ----
 * A fall looks like: free fall (|a| well below 1 g), then an impact
 * (|a| well above 1 g) shortly after, then the wearer lying still.
 * All three must happen in order, which keeps false alarms low when
 * the jacket is simply dropped on a chair or the wearer jumps.
 */
#define FALL_FREEFALL_G 0.45f
#define FALL_IMPACT_G 2.5f
#define FALL_IMPACT_WINDOW_MS 600u
#define FALL_SETTLE_MS 1000u
#define FALL_STILL_MS 2000u
#define FALL_STILL_STD_G 0.08f

typedef enum {
    FALL_IDLE = 0,
    FALL_FREEFALL,
    FALL_IMPACT,
    FALL_CHECK_STILL,
    FALL_CONFIRMED
} fall_state_t;

typedef struct {
    fall_state_t state;
    uint32_t t_mark_ms;
    float sum, sum_sq; /* stillness statistics */
    uint32_t n;
} fall_detector_t;

void fall_init(fall_detector_t *f);
/* Feed one accelerometer sample. Returns true once, when a fall is confirmed. */
bool fall_update(fall_detector_t *f, sj_vec3_t a, uint32_t now_ms);
void fall_reset(fall_detector_t *f);

/* ---- Activity features ----
 * A 2 second window of |a| is reduced to five numbers that the
 * on-device model classifies as still, walk or run.
 */
#define ACT_WINDOW (SJ_IMU_HZ * 2u)
#define ACT_FEATURES 5u

typedef struct {
    float mag[ACT_WINDOW];
    float z[ACT_WINDOW];
    uint32_t count; /* samples written since last window */
    uint32_t head;
    bool full;
} act_window_t;

void act_window_init(act_window_t *w);
/* Returns true when a full new window is ready (every 1 s, 50% overlap). */
bool act_window_push(act_window_t *w, sj_vec3_t a);
/* features: mean |a|, std |a|, range |a|, crossings per second, mean z */
void act_window_features(const act_window_t *w, float out[ACT_FEATURES]);

float vec3_mag(sj_vec3_t a);

#endif
