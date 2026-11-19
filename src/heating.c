#include "heating.h"

#include <string.h>

#define KP 8.0f
#define KI 0.4f
#define INTEG_LIMIT 150.0f

void heating_init(heating_t *h) { memset(h, 0, sizeof(*h)); }

void heating_set_level(heating_t *h, int level)
{
    if (level < 0) {
        level = 0;
    }
    if (level > HEAT_LEVEL_MAX) {
        level = HEAT_LEVEL_MAX;
    }
    h->level = level;
}

void heating_clear_faults(heating_t *h)
{
    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        h->zone[i].fault = HEAT_FAULT_NONE;
        h->zone[i].integ = 0.0f;
        h->zone[i].hard_on_ms = 0;
    }
}

const char *heating_fault_name(heat_fault_t f)
{
    switch (f) {
    case HEAT_FAULT_OVERTEMP: return "over-temperature";
    case HEAT_FAULT_SENSOR: return "sensor out of range";
    case HEAT_FAULT_NO_RISE: return "pad not warming";
    default: return "none";
    }
}

float heating_target(const heating_t *h, sj_zone_t z, const heat_inputs_t *in)
{
    if (h->level == 0) {
        return 0.0f;
    }
    float sp = 30.0f + 2.5f * (float)h->level; /* 32.5 .. 42.5 C */

    /* Colder outside: a little warmer, up to +3 C below 10 C ambient. */
    if (in->ambient_c < 10.0f) {
        float boost = (10.0f - in->ambient_c) * 0.2f;
        sp += (boost > 3.0f) ? 3.0f : boost;
    }
    /* Moving makes its own heat. */
    if (in->activity == SJ_ACT_WALK) {
        sp -= 1.0f;
    } else if (in->activity == SJ_ACT_RUN) {
        sp -= 4.0f;
    }
    if (in->skin_c > 36.0f) {
        sp -= 2.0f;
    }
    if (z == SJ_ZONE_COLLAR) {
        sp -= 2.0f; /* neck skin is more sensitive */
    }
    return (sp > HEAT_SETPOINT_MAX_C) ? HEAT_SETPOINT_MAX_C : sp;
}

uint8_t heating_update(heating_t *h, const heat_inputs_t *in, uint32_t dt_ms)
{
    uint8_t new_faults = 0;
    const float dt = (float)dt_ms / 1000.0f;

    for (uint32_t i = 0; i < SJ_HEAT_ZONES; i++) {
        heat_zone_t *zn = &h->zone[i];
        const float t = in->pad_c[i];
        const heat_fault_t before = zn->fault;

        /* --- Safety checks first, they latch. --- */
        if (t < HEAT_SENSOR_MIN_C || t > HEAT_SENSOR_MAX_C) {
            zn->fault = HEAT_FAULT_SENSOR;
        } else if (t >= HEAT_CUTOFF_C) {
            zn->fault = HEAT_FAULT_OVERTEMP;
        }

        if (zn->fault != HEAT_FAULT_NONE || h->level == 0) {
            zn->duty = 0;
            zn->integ = 0.0f;
            zn->hard_on_ms = 0;
            zn->setpoint = 0.0f;
            if (zn->fault != before) {
                new_faults |= (uint8_t)(1u << i);
            }
            continue;
        }

        /* --- PI controller --- */
        zn->setpoint = heating_target(h, (sj_zone_t)i, in);
        const float err = zn->setpoint - t;
        zn->integ += err * dt;
        if (zn->integ > INTEG_LIMIT) {
            zn->integ = INTEG_LIMIT;
        } else if (zn->integ < -INTEG_LIMIT) {
            zn->integ = -INTEG_LIMIT;
        }
        float out = KP * err + KI * zn->integ;
        if (out < 0.0f) {
            out = 0.0f;
        }
        if (out > 100.0f) {
            out = 100.0f;
        }
        uint8_t duty = (uint8_t)out;

        /* --- Safety limits that can only lower the duty --- */
        if (in->battery_pct <= HEAT_CRITICAL_BATTERY_PCT) {
            duty = 0;
        } else if (in->battery_pct <= HEAT_LOW_BATTERY_PCT && duty > HEAT_LOW_BATTERY_DUTY) {
            duty = HEAT_LOW_BATTERY_DUTY;
        }

        /* Heater driven hard for a minute with less than 1 C of rise: stop. */
        if (duty > 50u) {
            if (zn->hard_on_ms == 0u) {
                zn->rise_ref_c = t;
            }
            zn->hard_on_ms += dt_ms;
            if (t - zn->rise_ref_c >= 1.0f) {
                zn->hard_on_ms = 0; /* it is warming, start a fresh check */
            } else if (zn->hard_on_ms >= HEAT_NO_RISE_MS) {
                zn->fault = HEAT_FAULT_NO_RISE;
                duty = 0;
                new_faults |= (uint8_t)(1u << i);
            }
        } else {
            zn->hard_on_ms = 0;
        }
        zn->duty = duty;
    }
    return new_faults;
}
