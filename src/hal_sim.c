/* Simulated hardware: a wearer, the weather, three heater pads and a UART.
 * Deterministic for a given seed, so tests and demo output are repeatable. */
#include "hal_sim.h"

#include <math.h>
#include <string.h>

#include "hal.h"

#define PI_F 3.14159265f

static struct {
    uint32_t now_ms;
    uint32_t rng;
    sj_activity_t activity;
    float ambient_c;
    float pad_c[SJ_HEAT_ZONES];
    uint8_t duty[SJ_HEAT_ZONES];
    bool pad_broken[SJ_HEAT_ZONES];
    uint8_t battery;
    float hr_bpm;
    float pulse_phase;
    float gait_phase;
    uint32_t fall_start_ms;
    bool falling;
    bool lying;
    uint32_t button_until_ms;
    bool no_contact;
    uint32_t buzzes;
    float speech_phase;
    proto_parser_t tx_parser;
    sim_uart_sink_t sink;
    void *sink_ctx;
} S;

static float frand(void) /* 0..1, xorshift32 */
{
    S.rng ^= S.rng << 13;
    S.rng ^= S.rng >> 17;
    S.rng ^= S.rng << 5;
    return (float)(S.rng & 0xFFFFFFu) / (float)0x1000000u;
}

static float noise(float sd) /* roughly Gaussian */
{
    return sd * ((frand() + frand() + frand() + frand()) - 2.0f) * 1.7f;
}

void sim_init(uint32_t seed)
{
    memset(&S, 0, sizeof(S));
    S.rng = seed ? seed : 1u;
    S.ambient_c = 15.0f;
    S.battery = 80;
    S.hr_bpm = 68.0f;
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        S.pad_c[i] = 28.0f;
    }
    proto_parser_init(&S.tx_parser);
}

uint32_t sim_now_ms(void) { return S.now_ms; }

void sim_set_activity(sj_activity_t a)
{
    S.activity = a;
    S.lying = false;
}

void sim_trigger_fall(void)
{
    S.falling = true;
    S.fall_start_ms = S.now_ms;
}

void sim_set_ambient(float c) { S.ambient_c = c; }
void sim_set_battery(uint8_t pct) { S.battery = pct; }
void sim_break_pad_sensor(sj_zone_t z) { S.pad_broken[z] = true; }
void sim_press_button(uint32_t hold_ms) { S.button_until_ms = S.now_ms + hold_ms; }
void sim_remove_jacket_contact(bool removed) { S.no_contact = removed; }
uint8_t sim_heater_duty(sj_zone_t z) { return S.duty[z]; }
uint32_t sim_buzz_count(void) { return S.buzzes; }

void sim_set_uart_sink(sim_uart_sink_t sink, void *ctx)
{
    S.sink = sink;
    S.sink_ctx = ctx;
}

void sim_advance(uint32_t ms)
{
    const float dt = (float)ms / 1000.0f;
    S.now_ms += ms;

    /* Heart rate drifts toward what the activity needs. */
    const float target = (S.activity == SJ_ACT_RUN) ? 150.0f : (S.activity == SJ_ACT_WALK) ? 95.0f : 68.0f;
    S.hr_bpm += (target - S.hr_bpm) * (dt / 8.0f);

    /* Each pad: heater power in, heat loss to a mix of body and outside air. */
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        const float env = 0.6f * 33.0f + 0.4f * S.ambient_c;
        const float heat_in = (float)S.duty[i] / 100.0f * 0.9f; /* C per second at full power */
        S.pad_c[i] += (heat_in - (S.pad_c[i] - env) * 0.03f) * dt;
    }
}

/* ---- HAL implementation ---- */

bool hal_read_accel(sj_vec3_t *out)
{
    const float t = (float)S.now_ms / 1000.0f;
    if (S.falling) {
        const uint32_t since = S.now_ms - S.fall_start_ms;
        if (since < 300u) {
            *out = (sj_vec3_t){noise(0.05f), noise(0.05f), 0.1f + noise(0.05f)}; /* free fall */
            return true;
        }
        if (since < 320u) {
            *out = (sj_vec3_t){2.2f, 0.4f, 2.6f}; /* impact, about 3.4 g */
            return true;
        }
        if (since < 600u) {
            *out = (sj_vec3_t){1.0f + noise(0.4f), noise(0.4f), noise(0.4f)}; /* bounce */
            return true;
        }
        S.falling = false;
        S.lying = true;
        S.activity = SJ_ACT_STILL;
    }
    if (S.lying) {
        *out = (sj_vec3_t){1.0f + noise(0.02f), noise(0.02f), noise(0.02f)};
        return true;
    }
    if (S.activity == SJ_ACT_STILL) {
        *out = (sj_vec3_t){noise(0.02f), noise(0.02f), 1.0f + noise(0.02f)};
        return true;
    }
    const float f = (S.activity == SJ_ACT_WALK) ? 1.8f : 2.9f;
    const float amp = (S.activity == SJ_ACT_WALK) ? 0.3f : 0.9f;
    const float sd = (S.activity == SJ_ACT_WALK) ? 0.05f : 0.1f;
    const float ph = 2.0f * PI_F * f * t;
    *out = (sj_vec3_t){0.15f * amp * sinf(ph / 2.0f) + noise(sd), noise(sd), 1.0f + amp * sinf(ph) + noise(sd)};
    return true;
}

float hal_read_ppg(void)
{
    if (S.no_contact) {
        return 2000.0f + noise(3.0f);
    }
    S.pulse_phase += S.hr_bpm / 60.0f / (float)SJ_PPG_HZ;
    if (S.pulse_phase >= 1.0f) {
        S.pulse_phase -= 1.0f;
    }
    const float p = S.pulse_phase;
    /* Sharp systolic peak plus a small dicrotic bump, on a drifting baseline. */
    const float pulse = 100.0f * expf(-powf((p - 0.15f) / 0.06f, 2.0f)) +
                        25.0f * expf(-powf((p - 0.45f) / 0.08f, 2.0f));
    const float drift = 20.0f * sinf(2.0f * PI_F * 0.1f * (float)S.now_ms / 1000.0f);
    return 2000.0f + pulse + drift + noise(3.0f);
}

float hal_read_skin_temp(void)
{
    float avg_duty = 0.0f;
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        avg_duty += (float)S.duty[i] / (float)SJ_HEAT_ZONES;
    }
    float cold = (S.ambient_c < 15.0f) ? (15.0f - S.ambient_c) * 0.12f : 0.0f;
    return 34.0f - cold + avg_duty * 0.03f + noise(0.05f);
}

float hal_read_ambient_temp(void) { return S.ambient_c + noise(0.1f); }

float hal_read_pad_temp(sj_zone_t z)
{
    if (S.pad_broken[z]) {
        return -40.0f; /* typical reading of an open thermistor */
    }
    return S.pad_c[z] + noise(0.05f);
}

uint8_t hal_battery_pct(void) { return S.battery; }
bool hal_button_pressed(void) { return S.now_ms < S.button_until_ms; }

void hal_set_heater_duty(sj_zone_t z, uint8_t duty_pct) { S.duty[z] = duty_pct; }
void hal_haptic_buzz(uint16_t ms)
{
    (void)ms;
    S.buzzes++;
}

void hal_uart_write(const uint8_t *data, size_t len)
{
    proto_frame_t f;
    for (size_t i = 0; i < len; i++) {
        if (proto_feed(&S.tx_parser, data[i], &f) && S.sink) {
            S.sink(&f, S.sink_ctx);
        }
    }
}

void sim_audio_frame(int16_t *out, size_t n, bool speaking)
{
    for (size_t i = 0; i < n; i++) {
        float s = noise(60.0f); /* quiet room */
        if (speaking) {
            S.speech_phase += 2.0f * PI_F * 180.0f / (float)SJ_AUDIO_HZ;
            s += 2500.0f * sinf(S.speech_phase) + 1200.0f * sinf(2.7f * S.speech_phase);
        }
        out[i] = (int16_t)s;
    }
}
