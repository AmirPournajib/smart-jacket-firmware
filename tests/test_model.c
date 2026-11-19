#include <math.h>

#include "activity_model.h"
#include "test.h"

static act_result_t classify(float freq, float amp, float noise_seed)
{
    act_window_t w;
    act_window_init(&w);
    for (uint32_t i = 0; i < ACT_WINDOW; i++) {
        const float t = (float)i / (float)SJ_IMU_HZ;
        const float jitter = 0.01f * sinf(37.0f * t + noise_seed);
        const float z = 1.0f + amp * sinf(2.0f * 3.14159265f * freq * t) + jitter;
        act_window_push(&w, (sj_vec3_t){0.15f * amp * sinf(3.14159265f * freq * t), 0, z});
    }
    float f[ACT_FEATURES];
    act_window_features(&w, f);
    return activity_model_predict(f);
}

void test_model(void)
{
    const act_result_t still = classify(0.0f, 0.0f, 1.0f);
    const act_result_t walk = classify(1.8f, 0.3f, 2.0f);
    const act_result_t run = classify(2.9f, 0.9f, 3.0f);
    CHECK(still.activity == SJ_ACT_STILL);
    CHECK(walk.activity == SJ_ACT_WALK);
    CHECK(run.activity == SJ_ACT_RUN);
    CHECK(still.confidence > 0.5f && still.confidence <= 1.0f);
    CHECK(run.confidence > 0.5f);
}
