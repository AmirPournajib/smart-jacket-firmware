/* End-to-end: firmware + simulated hardware, checking what reaches the Jetson. */
#include "app.h"
#include "hal_sim.h"
#include "test.h"

typedef struct {
    int events[16];
    int status;
    int audio_start, audio_end;
} seen_t;

static void sink(const proto_frame_t *f, void *ctx)
{
    seen_t *s = (seen_t *)ctx;
    if (f->type == MSG_EVENT && f->payload[0] < 16u) {
        s->events[f->payload[0]]++;
    } else if (f->type == MSG_STATUS) {
        s->status++;
    } else if (f->type == MSG_AUDIO_START) {
        s->audio_start++;
    } else if (f->type == MSG_AUDIO_END) {
        s->audio_end++;
    }
}

static void run(app_t *a, uint32_t ms, bool speaking)
{
    int16_t pcm[SJ_AUDIO_FRAME];
    for (uint32_t i = 0; i < ms; i += 10u) {
        sim_advance(10);
        sim_audio_frame(pcm, SJ_AUDIO_FRAME, speaking);
        app_audio_frame(a, pcm, SJ_AUDIO_FRAME);
        if (sim_now_ms() % 20u == 0u) {
            app_sensor_tick(a, sim_now_ms());
        }
    }
}

static void send_intent(app_t *a, intent_t it)
{
    uint8_t frame[16];
    const uint8_t p[1] = {(uint8_t)it};
    app_uart_rx(a, frame, proto_encode(MSG_INTENT, p, 1, frame, sizeof(frame)));
}

void test_app(void)
{
    static app_t a;
    seen_t s = {{0}, 0, 0, 0};
    sim_init(7);
    sim_set_uart_sink(sink, &s);
    app_init(&a, 3);
    sim_set_ambient(0.0f);

    run(&a, 10000, false);
    CHECK(sim_heater_duty(SJ_ZONE_CHEST) > 0); /* cold, level 3: heating */

    /* Fall with no response: detected, then alert after the cancel window. */
    sim_trigger_fall();
    run(&a, 5000, false);
    CHECK(s.events[EVT_FALL_DETECTED] == 1);
    CHECK(s.events[EVT_FALL_ALERT_SENT] == 0);
    run(&a, FALL_CANCEL_WINDOW_MS, false);
    CHECK(s.events[EVT_FALL_ALERT_SENT] == 1);

    /* Fall cancelled by voice. */
    sim_set_activity(SJ_ACT_STILL);
    run(&a, 3000, false);
    sim_trigger_fall();
    run(&a, 5000, false);
    CHECK(s.events[EVT_FALL_DETECTED] == 2);
    send_intent(&a, INTENT_CANCEL_ALERT);
    CHECK(s.events[EVT_FALL_CANCELLED] == 1);
    run(&a, FALL_CANCEL_WINDOW_MS, false);
    CHECK(s.events[EVT_FALL_ALERT_SENT] == 1); /* no second alert */

    /* Speech produces exactly one segment. */
    sim_set_activity(SJ_ACT_STILL);
    run(&a, 1000, false);
    run(&a, 1200, true);
    run(&a, 1000, false);
    CHECK(s.audio_start == 1 && s.audio_end == 1);

    /* Voice intents. */
    send_intent(&a, INTENT_HEAT_UP);
    CHECK(a.heat.level == 4);
    send_intent(&a, INTENT_STATUS);
    CHECK(s.status == 1);
    send_intent(&a, INTENT_HEAT_OFF);
    run(&a, 2000, false);
    CHECK(sim_heater_duty(SJ_ZONE_CHEST) == 0);
    send_intent(&a, (intent_t)200); /* unknown: ignored */
    CHECK(a.heat.level == 0);
}
