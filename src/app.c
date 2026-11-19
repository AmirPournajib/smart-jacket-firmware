#include "app.h"

#include <math.h>
#include <string.h>

#include "hal.h"

static void send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[PROTO_MAX_PAYLOAD + PROTO_OVERHEAD];
    const size_t n = proto_encode(type, payload, len, frame, sizeof(frame));
    if (n > 0u) {
        hal_uart_write(frame, n);
    }
}

static void send_event(app_event_t e, uint8_t arg, uint8_t arg2)
{
    const uint8_t p[3] = {(uint8_t)e, arg, arg2};
    send(MSG_EVENT, p, sizeof(p));
}

static int16_t centi(float c) { return (int16_t)lroundf(c * 100.0f); }

void app_init(app_t *a, int heat_level)
{
    memset(a, 0, sizeof(*a));
    fall_init(&a->fall);
    act_window_init(&a->act);
    hr_init(&a->hr);
    heating_init(&a->heat);
    heating_set_level(&a->heat, heat_level);
    vad_init(&a->vad);
    proto_parser_init(&a->rx);
    a->activity.activity = SJ_ACT_STILL;
}

size_t app_build_telemetry(const app_t *a, uint8_t out[TLM_LEN])
{
    const float bpm = hr_bpm(&a->hr, a->now_ms);
    uint8_t faults = 0;
    out[0] = (uint8_t)a->activity.activity;
    out[1] = (uint8_t)lroundf(a->activity.confidence * 100.0f);
    out[2] = (uint8_t)((bpm > 255.0f) ? 255.0f : bpm);
    out[3] = hr_contact(&a->hr, a->now_ms) ? 1u : 0u;
    put_i16(&out[4], centi(a->last_skin_c));
    put_i16(&out[6], centi(a->last_ambient_c));
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        put_i16(&out[8 + 2 * i], centi(a->last_pad_c[i]));
        out[14 + i] = a->heat.zone[i].duty;
        if (a->heat.zone[i].fault != HEAT_FAULT_NONE) {
            faults |= (uint8_t)(1u << i);
        }
    }
    out[17] = hal_battery_pct();
    put_u16(&out[18], a->vital_flags);
    out[20] = (uint8_t)a->heat.level;
    out[21] = faults;
    return TLM_LEN;
}

static void handle_fall(app_t *a, sj_vec3_t acc)
{
    if (!a->fall_pending && fall_update(&a->fall, acc, a->now_ms)) {
        a->fall_pending = true;
        a->fall_at_ms = a->now_ms;
        a->activity.activity = SJ_ACT_STILL; /* lying down after the impact */
        a->activity.confidence = 1.0f;
        a->still_since_ms = a->now_ms;
        hal_haptic_buzz(800);
        send_event(EVT_FALL_DETECTED, (uint8_t)(FALL_CANCEL_WINDOW_MS / 1000u), 0);
    }
    if (a->fall_pending) {
        if (hal_button_pressed()) {
            a->fall_pending = false;
            fall_reset(&a->fall);
            send_event(EVT_FALL_CANCELLED, 0, 0);
        } else if (a->now_ms - a->fall_at_ms >= FALL_CANCEL_WINDOW_MS) {
            a->fall_pending = false;
            fall_reset(&a->fall);
            send_event(EVT_FALL_ALERT_SENT, 0, 0);
        }
    }
}

static void slow_tick(app_t *a)
{
    a->last_skin_c = hal_read_skin_temp();
    a->last_ambient_c = hal_read_ambient_temp();

    /* Vitals: report flags when they first appear. */
    const float bpm = hr_bpm(&a->hr, a->now_ms);
    const uint32_t still_ms = (a->activity.activity == SJ_ACT_STILL) ? a->now_ms - a->still_since_ms : 0u;
    const uint16_t flags = vitals_check(bpm, hr_contact(&a->hr, a->now_ms), a->activity.activity, still_ms,
                                        a->last_skin_c);
    const uint16_t fresh = (uint16_t)(flags & ~a->vital_flags);
    a->vital_flags = flags;
    if (fresh != 0u) {
        send_event(EVT_VITALS, (uint8_t)(fresh & 0xFFu), 0);
    }

    /* Heating. */
    heat_inputs_t in;
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        in.pad_c[i] = hal_read_pad_temp((sj_zone_t)i);
        a->last_pad_c[i] = in.pad_c[i];
    }
    in.ambient_c = a->last_ambient_c;
    in.skin_c = a->last_skin_c;
    in.activity = a->activity.activity;
    in.battery_pct = hal_battery_pct();
    const uint8_t new_faults = heating_update(&a->heat, &in, APP_SLOW_TICK_MS);
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        hal_set_heater_duty((sj_zone_t)i, a->heat.zone[i].duty);
        if (new_faults & (1u << i)) {
            send_event(EVT_HEAT_FAULT, (uint8_t)i, (uint8_t)a->heat.zone[i].fault);
        }
    }

    uint8_t tlm[TLM_LEN];
    send(MSG_TELEMETRY, tlm, (uint16_t)app_build_telemetry(a, tlm));
}

void app_sensor_tick(app_t *a, uint32_t now_ms)
{
    a->now_ms = now_ms;

    sj_vec3_t acc;
    if (hal_read_accel(&acc)) {
        handle_fall(a, acc);
        /* Skip classification while a fall is being checked: the impact and
         * bounce look like running and would only add noise to the log. */
        const bool fall_busy = a->fall_pending || a->fall.state >= FALL_IMPACT;
        if (act_window_push(&a->act, acc) && !fall_busy) {
            float f[ACT_FEATURES];
            act_window_features(&a->act, f);
            const act_result_t r = activity_model_predict(f);
            if (r.activity != a->activity.activity) {
                if (r.activity == SJ_ACT_STILL) {
                    a->still_since_ms = now_ms;
                }
                send_event(EVT_ACTIVITY, (uint8_t)r.activity, (uint8_t)lroundf(r.confidence * 100.0f));
            }
            a->activity = r;
        }
    }
    hr_update(&a->hr, hal_read_ppg(), now_ms);

    if (now_ms - a->last_slow_ms >= APP_SLOW_TICK_MS) {
        a->last_slow_ms = now_ms;
        slow_tick(a);
    }
}

void app_audio_frame(app_t *a, const int16_t *pcm, size_t n)
{
    const vad_event_t e = vad_process(&a->vad, pcm, n);
    if (e == VAD_START) {
        send(MSG_AUDIO_START, NULL, 0);
    }
    if (e == VAD_START || e == VAD_CONTINUE) {
        uint8_t buf[SJ_AUDIO_FRAME * 2u];
        const size_t m = (n > SJ_AUDIO_FRAME) ? SJ_AUDIO_FRAME : n;
        for (size_t i = 0; i < m; i++) {
            put_i16(&buf[2 * i], pcm[i]);
        }
        send(MSG_AUDIO_CHUNK, buf, (uint16_t)(2u * m));
        a->audio_frames_sent++;
    } else if (e == VAD_END) {
        send(MSG_AUDIO_END, NULL, 0);
    }
}

static void apply_intent(app_t *a, intent_t intent)
{
    switch (intent) {
    case INTENT_HEAT_UP:
        heating_set_level(&a->heat, a->heat.level + 1);
        send_event(EVT_HEAT_LEVEL, (uint8_t)a->heat.level, 0);
        break;
    case INTENT_HEAT_DOWN:
        heating_set_level(&a->heat, a->heat.level - 1);
        send_event(EVT_HEAT_LEVEL, (uint8_t)a->heat.level, 0);
        break;
    case INTENT_HEAT_OFF:
        heating_set_level(&a->heat, 0);
        send_event(EVT_HEAT_LEVEL, 0, 0);
        break;
    case INTENT_STATUS: {
        uint8_t tlm[TLM_LEN];
        send(MSG_STATUS, tlm, (uint16_t)app_build_telemetry(a, tlm));
        break;
    }
    case INTENT_CANCEL_ALERT:
        if (a->fall_pending) {
            a->fall_pending = false;
            fall_reset(&a->fall);
            send_event(EVT_FALL_CANCELLED, 1, 0);
        }
        break;
    default:
        break; /* unknown intents are ignored, never guessed */
    }
}

void app_uart_rx(app_t *a, const uint8_t *data, size_t len)
{
    proto_frame_t f;
    for (size_t i = 0; i < len; i++) {
        if (proto_feed(&a->rx, data[i], &f) && f.type == MSG_INTENT && f.len >= 1u) {
            apply_intent(a, (intent_t)f.payload[0]);
        }
    }
}
