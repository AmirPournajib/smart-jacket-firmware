#include <math.h>

#include "test.h"
#include "voice.h"

static void frame(int16_t *buf, float amp, uint32_t k)
{
    for (uint32_t i = 0; i < SJ_AUDIO_FRAME; i++) {
        const float n = (float)(((i * 7919u + k * 104729u) % 200u)) - 100.0f; /* quiet room noise */
        buf[i] = (int16_t)(n * 0.6f + amp * sinf(0.07f * (float)(i + k * SJ_AUDIO_FRAME)));
    }
}

void test_voice(void)
{
    vad_t v;
    vad_init(&v);
    int16_t buf[SJ_AUDIO_FRAME];
    int starts = 0, ends = 0, cont = 0;
    uint32_t k = 0;

    for (; k < 100; k++) { /* 1 s of silence */
        frame(buf, 0.0f, k);
        CHECK(vad_process(&v, buf, SJ_AUDIO_FRAME) == VAD_NONE);
    }
    for (; k < 200; k++) { /* 1 s of speech */
        frame(buf, 3000.0f, k);
        const vad_event_t e = vad_process(&v, buf, SJ_AUDIO_FRAME);
        starts += (e == VAD_START);
        cont += (e == VAD_CONTINUE);
    }
    for (; k < 300; k++) { /* 1 s of silence again */
        frame(buf, 0.0f, k);
        ends += (vad_process(&v, buf, SJ_AUDIO_FRAME) == VAD_END);
    }
    CHECK(starts == 1);
    CHECK(ends == 1);
    CHECK(cont > 90);

    /* A single click is not speech. */
    vad_init(&v);
    for (k = 0; k < 50; k++) {
        frame(buf, (k == 25) ? 5000.0f : 0.0f, k);
        CHECK(vad_process(&v, buf, SJ_AUDIO_FRAME) == VAD_NONE);
    }
    CHECK(intent_name((intent_t)99)[0] == 'u');
}
