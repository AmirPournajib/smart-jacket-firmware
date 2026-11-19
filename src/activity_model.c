#include "activity_model.h"

#include <math.h>

#include "activity_weights.h"

static int32_t clamp_i8(float v, int32_t lo, int32_t hi)
{
    int32_t q = (int32_t)lroundf(v);
    if (q < lo) {
        return lo;
    }
    if (q > hi) {
        return hi;
    }
    return q;
}

act_result_t activity_model_predict(const float features[ACT_FEATURES])
{
    int32_t qin[ACT_FEATURES];
    int32_t qh[ACT_HIDDEN];
    int32_t acc2[SJ_ACT_COUNT];

    /* 1. Standardize and quantize the input to int8. */
    for (uint32_t i = 0; i < ACT_FEATURES; i++) {
        const float xn = (features[i] - ACT_MU[i]) / ACT_SIGMA[i];
        qin[i] = clamp_i8(xn / ACT_S_IN, -127, 127);
    }

    /* 2. Hidden layer: int8 x int8 -> int32, ReLU, requantize to int8. */
    for (uint32_t j = 0; j < ACT_HIDDEN; j++) {
        int32_t acc = ACT_B1[j];
        for (uint32_t i = 0; i < ACT_FEATURES; i++) {
            acc += qin[i] * (int32_t)ACT_W1[i * ACT_HIDDEN + j];
        }
        const float h = (acc > 0) ? (float)acc * (ACT_S_IN * ACT_S_W1) : 0.0f;
        qh[j] = clamp_i8(h / ACT_S_H, 0, 127);
    }

    /* 3. Output layer. */
    for (uint32_t k = 0; k < SJ_ACT_COUNT; k++) {
        int32_t acc = ACT_B2[k];
        for (uint32_t j = 0; j < ACT_HIDDEN; j++) {
            acc += qh[j] * (int32_t)ACT_W2[j * SJ_ACT_COUNT + k];
        }
        acc2[k] = acc;
    }

    /* 4. Argmax plus a softmax confidence for the app and the review gate. */
    uint32_t best = 0;
    for (uint32_t k = 1; k < SJ_ACT_COUNT; k++) {
        if (acc2[k] > acc2[best]) {
            best = k;
        }
    }
    const float scale = ACT_S_H * ACT_S_W2;
    float denom = 0.0f;
    for (uint32_t k = 0; k < SJ_ACT_COUNT; k++) {
        denom += expf((float)(acc2[k] - acc2[best]) * scale);
    }

    act_result_t r = {(sj_activity_t)best, 1.0f / denom};
    return r;
}
