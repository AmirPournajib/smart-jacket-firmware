#include "voice.h"

#include <string.h>

void vad_init(vad_t *v) { memset(v, 0, sizeof(*v)); }

static float frame_energy(const int16_t *frame, size_t n)
{
    float e = 0.0f;
    for (size_t i = 0; i < n; i++) {
        const float s = (float)frame[i];
        e += s * s;
    }
    return (n > 0u) ? e / (float)n : 0.0f;
}

vad_event_t vad_process(vad_t *v, const int16_t *frame, size_t n)
{
    const float e = frame_energy(frame, n);
    if (!v->primed) {
        v->noise_floor = e;
        v->primed = true;
    }
    const bool speech = e > VAD_MIN_ENERGY && e > VAD_RATIO * v->noise_floor;

    if (!v->active) {
        /* Track the room's noise level only while nobody is talking. */
        if (!speech) {
            v->noise_floor += 0.05f * (e - v->noise_floor);
        }
        v->speech_run = speech ? v->speech_run + 1u : 0u;
        if (v->speech_run >= VAD_START_FRAMES) {
            v->active = true;
            v->silence_run = 0;
            return VAD_START;
        }
        return VAD_NONE;
    }

    v->silence_run = speech ? 0u : v->silence_run + 1u;
    if (v->silence_run >= VAD_HANGOVER_FRAMES) {
        v->active = false;
        v->speech_run = 0;
        return VAD_END;
    }
    return VAD_CONTINUE;
}

const char *intent_name(intent_t i)
{
    static const char *names[INTENT_COUNT] = {"none", "heat up", "heat down", "heat off", "status",
                                              "cancel alert"};
    return (i < INTENT_COUNT) ? names[i] : "unknown";
}
