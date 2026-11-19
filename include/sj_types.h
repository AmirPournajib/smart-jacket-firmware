/* Shared types and constants for the smart jacket firmware. */
#ifndef SJ_TYPES_H
#define SJ_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SJ_IMU_HZ 50u          /* accelerometer sample rate */
#define SJ_PPG_HZ 50u          /* optical heart rate sample rate */
#define SJ_AUDIO_HZ 16000u     /* microphone sample rate */
#define SJ_AUDIO_FRAME 160u    /* 10 ms audio frame */
#define SJ_HEAT_ZONES 3u       /* chest, back, collar */

typedef struct {
    float x, y, z; /* acceleration in g */
} sj_vec3_t;

typedef enum {
    SJ_ACT_STILL = 0,
    SJ_ACT_WALK = 1,
    SJ_ACT_RUN = 2,
    SJ_ACT_COUNT
} sj_activity_t;

typedef enum {
    SJ_ZONE_CHEST = 0,
    SJ_ZONE_BACK = 1,
    SJ_ZONE_COLLAR = 2
} sj_zone_t;

const char *sj_activity_name(sj_activity_t a);
const char *sj_zone_name(sj_zone_t z);

#endif
