/* Runs the firmware against the simulated world and prints what the Jetson
 * would receive. Also stands in for the Jetson: when a speech segment ends it
 * answers with the next scripted intent, the way the real Jetson would after
 * wake word, speech-to-text and the local language model.
 *
 *   make sim && ./build/jacket_sim
 */
#include <stdio.h>

#include "app.h"
#include "hal_sim.h"

typedef struct {
    app_t *app;
    const intent_t *script;
    size_t script_len;
    size_t next;
    int telemetry_every;
    int telemetry_seen;
    int events;
    int audio_chunks;
} jetson_t;

static const char *event_text(const uint8_t *p, char *buf, size_t cap)
{
    switch ((app_event_t)p[0]) {
    case EVT_FALL_DETECTED:
        snprintf(buf, cap, "FALL DETECTED, buzzing; wearer has %u s to cancel", p[1]);
        break;
    case EVT_FALL_CANCELLED:
        snprintf(buf, cap, "fall alert cancelled by %s", p[1] ? "voice" : "button");
        break;
    case EVT_FALL_ALERT_SENT:
        snprintf(buf, cap, "no response: ALERT sent to emergency contact");
        break;
    case EVT_VITALS:
        snprintf(buf, cap, "vitals flags 0x%02X%s%s%s%s", p[1], (p[1] & VITAL_HR_HIGH_AT_REST) ? " hr-high-at-rest" : "",
                 (p[1] & VITAL_HR_LOW) ? " hr-low" : "", (p[1] & VITAL_SKIN_COLD) ? " skin-cold" : "",
                 (p[1] & VITAL_NO_CONTACT) ? " no-contact" : "");
        break;
    case EVT_HEAT_FAULT:
        snprintf(buf, cap, "HEATER FAULT in %s zone: %s, zone switched off", sj_zone_name((sj_zone_t)p[1]),
                 heating_fault_name((heat_fault_t)p[2]));
        break;
    case EVT_ACTIVITY:
        snprintf(buf, cap, "activity -> %s (%u%% confident)", sj_activity_name((sj_activity_t)p[1]), p[2]);
        break;
    case EVT_HEAT_LEVEL:
        snprintf(buf, cap, "heat level -> %u", p[1]);
        break;
    default:
        snprintf(buf, cap, "event %u", p[0]);
    }
    return buf;
}

static void print_telemetry(const char *tag, const uint8_t *p)
{
    printf("%7.1fs  %-9s %-5s hr=%3u skin=%.1fC air=%.1fC pads=%.1f/%.1f/%.1fC duty=%u/%u/%u%% batt=%u%% level=%u\n",
           sim_now_ms() / 1000.0, tag, sj_activity_name((sj_activity_t)p[0]), p[2], get_i16(&p[4]) / 100.0,
           get_i16(&p[6]) / 100.0, get_i16(&p[8]) / 100.0, get_i16(&p[10]) / 100.0, get_i16(&p[12]) / 100.0,
           p[14], p[15], p[16], p[17], p[20]);
}

static void on_frame(const proto_frame_t *f, void *ctx)
{
    jetson_t *j = (jetson_t *)ctx;
    char buf[160];
    switch (f->type) {
    case MSG_TELEMETRY:
        if (++j->telemetry_seen % j->telemetry_every == 0) {
            print_telemetry("telemetry", f->payload);
        }
        break;
    case MSG_STATUS:
        print_telemetry("STATUS", f->payload);
        break;
    case MSG_EVENT:
        j->events++;
        printf("%7.1fs  EVENT     %s\n", sim_now_ms() / 1000.0, event_text(f->payload, buf, sizeof(buf)));
        break;
    case MSG_AUDIO_START:
        printf("%7.1fs  voice     speech started, Jetson wakes up\n", sim_now_ms() / 1000.0);
        break;
    case MSG_AUDIO_CHUNK:
        j->audio_chunks++;
        break;
    case MSG_AUDIO_END: {
        printf("%7.1fs  voice     speech ended (%d x 10 ms chunks streamed)\n", sim_now_ms() / 1000.0, j->audio_chunks);
        j->audio_chunks = 0;
        if (j->next < j->script_len) {
            const intent_t it = j->script[j->next++];
            printf("%7.1fs  jetson    understood: \"%s\"\n", sim_now_ms() / 1000.0, intent_name(it));
            const uint8_t payload[1] = {(uint8_t)it};
            uint8_t frame[16];
            const size_t n = proto_encode(MSG_INTENT, payload, 1, frame, sizeof(frame));
            app_uart_rx(j->app, frame, n);
        }
        break;
    }
    default:
        break;
    }
}

/* Run the firmware for `seconds`, with optional speech in [speak_from, speak_to). */
static void run(app_t *app, double seconds, double speak_from, double speak_to)
{
    const uint32_t end = sim_now_ms() + (uint32_t)(seconds * 1000.0);
    int16_t pcm[SJ_AUDIO_FRAME];
    while (sim_now_ms() < end) {
        sim_advance(10);
        const double t = sim_now_ms() / 1000.0;
        sim_audio_frame(pcm, SJ_AUDIO_FRAME, t >= speak_from && t < speak_to);
        app_audio_frame(app, pcm, SJ_AUDIO_FRAME);
        if (sim_now_ms() % (1000u / SJ_IMU_HZ) == 0u) {
            app_sensor_tick(app, sim_now_ms());
        }
    }
}

int main(void)
{
    static app_t app;
    static const intent_t script[] = {INTENT_HEAT_UP, INTENT_STATUS};
    jetson_t jetson = {&app, script, 2, 0, 10, 0, 0, 0};

    sim_init(42);
    sim_set_uart_sink(on_frame, &jetson);
    app_init(&app, 3);

    printf("Smart jacket firmware simulation (synthetic sensors)\n\n");
    printf("--- Standing outside at -2 C, heat level 3 ---\n");
    sim_set_ambient(-2.0f);
    run(&app, 20, -1, -1);

    printf("--- Walking ---\n");
    sim_set_activity(SJ_ACT_WALK);
    run(&app, 25, -1, -1);

    printf("--- Running: heart rate climbs, heaters back off ---\n");
    sim_set_activity(SJ_ACT_RUN);
    run(&app, 25, -1, -1);

    printf("--- Fall, wearer does not respond ---\n");
    sim_trigger_fall();
    run(&app, 22, -1, -1);

    printf("--- Wearer gets up and says \"make it warmer\", then asks for status ---\n");
    sim_set_activity(SJ_ACT_STILL);
    {
        const double t0 = sim_now_ms() / 1000.0;
        run(&app, 4, t0 + 1.0, t0 + 2.2);
        const double t1 = sim_now_ms() / 1000.0;
        run(&app, 4, t1 + 1.0, t1 + 2.0);
    }

    printf("--- Trips, but presses the button to cancel ---\n");
    sim_trigger_fall();
    run(&app, 5, -1, -1);
    sim_press_button(1500);
    run(&app, 5, -1, -1);
    sim_set_activity(SJ_ACT_STILL);

    printf("--- Collar temperature sensor fails ---\n");
    sim_break_pad_sensor(SJ_ZONE_COLLAR);
    run(&app, 5, -1, -1);

    printf("--- Battery drops to 12%%: heater power capped ---\n");
    sim_set_battery(12);
    run(&app, 10, -1, -1);

    printf("\n%d events, %u frames parsed OK, %u CRC errors, %u haptic alerts\n", jetson.events,
           app.rx.frames_ok, app.rx.crc_errors, sim_buzz_count());
    return 0;
}
