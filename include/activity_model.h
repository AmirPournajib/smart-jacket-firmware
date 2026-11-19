/* On-device activity classifier: int8 MLP (Multi-Layer Perceptron), 5 -> 8 -> 3. */
#ifndef ACTIVITY_MODEL_H
#define ACTIVITY_MODEL_H

#include "imu.h"
#include "sj_types.h"

typedef struct {
    sj_activity_t activity;
    float confidence; /* softmax probability of the chosen class, 0..1 */
} act_result_t;

/* Integer math in the layers, float only for input scaling and the final softmax.
 * Runs in well under a millisecond on a Cortex-M4 class MCU (Microcontroller Unit). */
act_result_t activity_model_predict(const float features[ACT_FEATURES]);

#endif
