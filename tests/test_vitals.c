#include <math.h>

#include "test.h"
#include "vitals.h"

static float run_hr(float bpm, uint32_t seconds)
{
    hr_estimator_t h;
    hr_init(&h);
    float phase = 0.0f;
    uint32_t t = 0;
    for (uint32_t i = 0; i < seconds * SJ_PPG_HZ; i++) {
        phase += bpm / 60.0f / (float)SJ_PPG_HZ;
        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
        const float ppg = 1500.0f + 100.0f * expf(-powf((phase - 0.15f) / 0.06f, 2.0f));
        t += 1000u / SJ_PPG_HZ;
        hr_update(&h, ppg, t);
    }
    return hr_bpm(&h, t);
}

void test_vitals(void)
{
    CHECK_NEAR(run_hr(60.0f, 20), 60.0f, 3.0f);
    CHECK_NEAR(run_hr(90.0f, 20), 90.0f, 4.0f);
    CHECK_NEAR(run_hr(150.0f, 20), 150.0f, 8.0f);

    /* No pulse at all: no reading, and contact is reported lost. */
    hr_estimator_t h;
    hr_init(&h);
    for (uint32_t t = 20; t < 6000; t += 20) {
        hr_update(&h, 1500.0f, t);
    }
    CHECK(hr_bpm(&h, 6000) == 0.0f);
    CHECK(!hr_contact(&h, 6000));

    /* Alerts */
    CHECK(vitals_check(130, true, SJ_ACT_STILL, 120000, 34) & VITAL_HR_HIGH_AT_REST);
    CHECK(!(vitals_check(130, true, SJ_ACT_STILL, 10000, 34) & VITAL_HR_HIGH_AT_REST)); /* just stopped */
    CHECK(!(vitals_check(150, true, SJ_ACT_RUN, 0, 34) & VITAL_HR_HIGH_AT_REST));
    CHECK(vitals_check(35, true, SJ_ACT_STILL, 0, 34) & VITAL_HR_LOW);
    CHECK(vitals_check(70, true, SJ_ACT_STILL, 0, 30) & VITAL_SKIN_COLD);
    CHECK(vitals_check(0, false, SJ_ACT_STILL, 0, 34) == VITAL_NO_CONTACT);
}
