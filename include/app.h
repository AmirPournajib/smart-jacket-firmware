/* Application: ties sensing, the on-device model, heating, voice and the
 * Jetson link together. Pure logic on top of hal.h, so it runs the same on
 * the board and in the simulator. */
#ifndef APP_H
#define APP_H

#include "activity_model.h"
#include "heating.h"
#include "imu.h"
#include "protocol.h"
#include "vitals.h"
#include "voice.h"

#define APP_SLOW_TICK_MS 1000u
#define FALL_CANCEL_WINDOW_MS 15000u

typedef enum {
    EVT_FALL_DETECTED = 1,   /* arg: seconds the wearer has to cancel */
    EVT_FALL_CANCELLED = 2,  /* arg: 0 button, 1 voice */
    EVT_FALL_ALERT_SENT = 3, /* Jetson/phone forwards to emergency contact */
    EVT_VITALS = 4,          /* arg: new vital flags (low byte) */
    EVT_HEAT_FAULT = 5,      /* arg: zone, arg2: fault code */
    EVT_ACTIVITY = 6,        /* arg: activity, arg2: confidence % */
    EVT_HEAT_LEVEL = 7       /* arg: new level */
} app_event_t;

/* Telemetry / status payload layout (little endian), 22 bytes. */
#define TLM_LEN 22u

typedef struct {
    fall_detector_t fall;
    act_window_t act;
    hr_estimator_t hr;
    heating_t heat;
    vad_t vad;
    proto_parser_t rx;

    act_result_t activity;
    uint16_t vital_flags;
    bool fall_pending;
    uint32_t fall_at_ms;
    uint32_t last_slow_ms;
    uint32_t now_ms;
    uint32_t still_since_ms;
    uint32_t audio_frames_sent;
    float last_skin_c;
    float last_ambient_c;
    float last_pad_c[SJ_HEAT_ZONES];
} app_t;

void app_init(app_t *a, int heat_level);
void app_sensor_tick(app_t *a, uint32_t now_ms);                 /* call at SJ_IMU_HZ */
void app_audio_frame(app_t *a, const int16_t *pcm, size_t n);     /* call every 10 ms */
void app_uart_rx(app_t *a, const uint8_t *data, size_t len);     /* bytes from the Jetson */
size_t app_build_telemetry(const app_t *a, uint8_t out[TLM_LEN]);

#endif
