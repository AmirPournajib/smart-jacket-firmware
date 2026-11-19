/* Heart rate from the optical (PPG, photoplethysmography) sensor, plus wellness alerts.
 * These are comfort and safety hints for the wearer, not medical measurements. */
#ifndef VITALS_H
#define VITALS_H

#include "sj_types.h"

#define HR_IBI_HISTORY 8u
#define HR_MIN_IBI_MS 300u   /* 200 bpm */
#define HR_MAX_IBI_MS 2000u  /* 30 bpm */
#define HR_CONTACT_TIMEOUT_MS 4000u

typedef struct {
    float baseline;   /* slow average, removes the DC level */
    float envelope;   /* decaying peak amplitude for the adaptive threshold */
    float prev, prev2;
    uint32_t last_beat_ms;
    uint32_t ibi_ms[HR_IBI_HISTORY];
    uint8_t ibi_count;
    uint8_t ibi_head;
    bool primed;
} hr_estimator_t;

void hr_init(hr_estimator_t *h);
/* Feed one PPG sample. Returns true when a beat is detected. */
bool hr_update(hr_estimator_t *h, float ppg, uint32_t now_ms);
/* Beats per minute, or 0 when there is no reliable reading. */
float hr_bpm(const hr_estimator_t *h, uint32_t now_ms);
bool hr_contact(const hr_estimator_t *h, uint32_t now_ms);

/* Alert flags, sent with telemetry and as events when they first appear. */
enum {
    VITAL_OK = 0,
    VITAL_HR_HIGH_AT_REST = 1u << 0,
    VITAL_HR_LOW = 1u << 1,
    VITAL_SKIN_COLD = 1u << 2,
    VITAL_NO_CONTACT = 1u << 3
};

#define VITAL_HR_REST_HIGH_BPM 120.0f
#define VITAL_HR_LOW_BPM 40.0f
#define VITAL_SKIN_COLD_C 32.0f
#define VITAL_REST_MS 60000u /* heart rate needs a minute to settle after exercise */

uint16_t vitals_check(float bpm, bool contact, sj_activity_t activity, uint32_t still_ms, float skin_c);

#endif
