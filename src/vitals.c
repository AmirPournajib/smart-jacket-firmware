#include "vitals.h"

#include <string.h>

void hr_init(hr_estimator_t *h) { memset(h, 0, sizeof(*h)); }

bool hr_update(hr_estimator_t *h, float ppg, uint32_t now_ms)
{
    if (!h->primed) {
        h->baseline = ppg;
        h->primed = true;
        return false;
    }
    h->baseline += 0.02f * (ppg - h->baseline);
    const float x = ppg - h->baseline;
    const float ax = (x > 0.0f) ? x : -x;
    h->envelope = (ax > h->envelope) ? ax : h->envelope * 0.995f;

    bool beat = false;
    /* Local maximum one sample ago, above half the recent envelope,
     * and far enough from the last beat to be physiologically possible. */
    if (h->prev > h->prev2 && h->prev >= x && h->prev > 0.5f * h->envelope &&
        (h->last_beat_ms == 0u || now_ms - h->last_beat_ms >= HR_MIN_IBI_MS)) {
        if (h->last_beat_ms != 0u) {
            const uint32_t ibi = now_ms - h->last_beat_ms;
            if (ibi <= HR_MAX_IBI_MS) {
                h->ibi_ms[h->ibi_head] = ibi;
                h->ibi_head = (uint8_t)((h->ibi_head + 1u) % HR_IBI_HISTORY);
                if (h->ibi_count < HR_IBI_HISTORY) {
                    h->ibi_count++;
                }
            }
        }
        h->last_beat_ms = now_ms;
        beat = true;
    }
    h->prev2 = h->prev;
    h->prev = x;
    return beat;
}

bool hr_contact(const hr_estimator_t *h, uint32_t now_ms)
{
    return h->last_beat_ms != 0u && now_ms - h->last_beat_ms < HR_CONTACT_TIMEOUT_MS;
}

float hr_bpm(const hr_estimator_t *h, uint32_t now_ms)
{
    if (h->ibi_count < 3u || !hr_contact(h, now_ms)) {
        return 0.0f;
    }
    /* Drop the single longest and shortest interval to resist one missed or extra beat. */
    uint32_t sum = 0, lo = UINT32_MAX, hi = 0;
    for (uint8_t i = 0; i < h->ibi_count; i++) {
        const uint32_t v = h->ibi_ms[i];
        sum += v;
        lo = (v < lo) ? v : lo;
        hi = (v > hi) ? v : hi;
    }
    if (h->ibi_count >= 5u) {
        sum -= lo + hi;
        return 60000.0f / ((float)sum / (float)(h->ibi_count - 2u));
    }
    return 60000.0f / ((float)sum / (float)h->ibi_count);
}

uint16_t vitals_check(float bpm, bool contact, sj_activity_t activity, uint32_t still_ms, float skin_c)
{
    uint16_t flags = VITAL_OK;
    if (!contact) {
        flags |= VITAL_NO_CONTACT;
    } else if (bpm > 0.0f) {
        if (activity == SJ_ACT_STILL && still_ms >= VITAL_REST_MS && bpm > VITAL_HR_REST_HIGH_BPM) {
            flags |= VITAL_HR_HIGH_AT_REST;
        }
        if (bpm < VITAL_HR_LOW_BPM) {
            flags |= VITAL_HR_LOW;
        }
    }
    if (skin_c < VITAL_SKIN_COLD_C) {
        flags |= VITAL_SKIN_COLD;
    }
    return flags;
}
