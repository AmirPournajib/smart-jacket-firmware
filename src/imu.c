#include "imu.h"

#include <math.h>
#include <string.h>

float vec3_mag(sj_vec3_t a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

void fall_init(fall_detector_t *f) { memset(f, 0, sizeof(*f)); }

void fall_reset(fall_detector_t *f) { fall_init(f); }

bool fall_update(fall_detector_t *f, sj_vec3_t a, uint32_t now_ms)
{
    const float m = vec3_mag(a);

    switch (f->state) {
    case FALL_IDLE:
        if (m < FALL_FREEFALL_G) {
            f->state = FALL_FREEFALL;
            f->t_mark_ms = now_ms;
        }
        break;

    case FALL_FREEFALL:
        if (m > FALL_IMPACT_G) {
            f->state = FALL_IMPACT;
            f->t_mark_ms = now_ms;
        } else if (now_ms - f->t_mark_ms > FALL_IMPACT_WINDOW_MS) {
            f->state = FALL_IDLE; /* dropped, jumped or noise: no impact followed */
        }
        break;

    case FALL_IMPACT:
        /* Ignore the bounce right after the impact, then start measuring. */
        if (now_ms - f->t_mark_ms >= FALL_SETTLE_MS) {
            f->state = FALL_CHECK_STILL;
            f->t_mark_ms = now_ms;
            f->sum = f->sum_sq = 0.0f;
            f->n = 0;
        }
        break;

    case FALL_CHECK_STILL:
        f->sum += m;
        f->sum_sq += m * m;
        f->n++;
        if (now_ms - f->t_mark_ms >= FALL_STILL_MS) {
            const float mean = f->sum / (float)f->n;
            float var = f->sum_sq / (float)f->n - mean * mean;
            if (var < 0.0f) {
                var = 0.0f;
            }
            if (sqrtf(var) < FALL_STILL_STD_G) {
                f->state = FALL_CONFIRMED;
                return true;
            }
            f->state = FALL_IDLE; /* wearer got up and moved on */
        }
        break;

    case FALL_CONFIRMED:
        /* Stays latched until the app handles the alert and calls fall_reset. */
        break;
    }
    return false;
}

void act_window_init(act_window_t *w) { memset(w, 0, sizeof(*w)); }

bool act_window_push(act_window_t *w, sj_vec3_t a)
{
    w->mag[w->head] = vec3_mag(a);
    w->z[w->head] = a.z;
    w->head = (w->head + 1u) % ACT_WINDOW;
    if (w->head == 0u) {
        w->full = true;
    }
    w->count++;
    if (w->full && w->count >= ACT_WINDOW / 2u) {
        w->count = 0;
        return true;
    }
    return false;
}

void act_window_features(const act_window_t *w, float out[ACT_FEATURES])
{
    float sum = 0.0f, sum_sq = 0.0f, zsum = 0.0f;
    float lo = w->mag[0], hi = w->mag[0];

    for (uint32_t i = 0; i < ACT_WINDOW; i++) {
        const float m = w->mag[i];
        sum += m;
        sum_sq += m * m;
        zsum += w->z[i];
        if (m < lo) {
            lo = m;
        }
        if (m > hi) {
            hi = m;
        }
    }
    const float mean = sum / (float)ACT_WINDOW;
    float var = sum_sq / (float)ACT_WINDOW - mean * mean;
    if (var < 0.0f) {
        var = 0.0f;
    }

    /* Mean crossings, with a small dead band so sensor noise is not counted. */
    uint32_t crossings = 0;
    int prev = 0;
    for (uint32_t k = 0; k < ACT_WINDOW; k++) {
        const float d = w->mag[(w->head + k) % ACT_WINDOW] - mean;
        const int s = (d > 0.05f) ? 1 : (d < -0.05f) ? -1 : 0;
        if (s != 0) {
            if (prev != 0 && s != prev) {
                crossings++;
            }
            prev = s;
        }
    }

    out[0] = mean;
    out[1] = sqrtf(var);
    out[2] = hi - lo;
    out[3] = (float)crossings / 2.0f; /* window is 2 s, so this is per second */
    out[4] = zsum / (float)ACT_WINDOW;
}
