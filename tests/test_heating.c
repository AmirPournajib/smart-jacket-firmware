#include "heating.h"
#include "test.h"

static heat_inputs_t inputs(float pad, float ambient)
{
    heat_inputs_t in = {{pad, pad, pad}, ambient, 34.0f, SJ_ACT_STILL, 80};
    return in;
}

void test_heating(void)
{
    heating_t h;
    heating_init(&h);
    heat_inputs_t in = inputs(25.0f, 0.0f);

    /* Off means off. */
    heating_update(&h, &in, 1000);
    CHECK(h.zone[0].duty == 0);

    /* Cold pad at level 3 heats hard; collar target is lower than chest. */
    heating_set_level(&h, 3);
    heating_update(&h, &in, 1000);
    CHECK(h.zone[0].duty > 80);
    CHECK(heating_target(&h, SJ_ZONE_COLLAR, &in) < heating_target(&h, SJ_ZONE_CHEST, &in));

    /* Running lowers the target. */
    heat_inputs_t run = in;
    run.activity = SJ_ACT_RUN;
    CHECK(heating_target(&h, SJ_ZONE_CHEST, &run) < heating_target(&h, SJ_ZONE_CHEST, &in));

    /* Target never exceeds the cap, even at max level in extreme cold. */
    heating_set_level(&h, 99);
    CHECK(h.level == HEAT_LEVEL_MAX);
    heat_inputs_t arctic = inputs(25.0f, -40.0f);
    CHECK(heating_target(&h, SJ_ZONE_CHEST, &arctic) <= HEAT_SETPOINT_MAX_C);

    /* Over-temperature latches the zone off. */
    heating_init(&h);
    heating_set_level(&h, 3);
    heat_inputs_t hot = inputs(25.0f, 0.0f);
    hot.pad_c[1] = 49.0f;
    uint8_t faults = heating_update(&h, &hot, 1000);
    CHECK(faults == (1u << 1));
    CHECK(h.zone[1].fault == HEAT_FAULT_OVERTEMP && h.zone[1].duty == 0);
    hot.pad_c[1] = 30.0f; /* cooled down, still latched until cleared */
    heating_update(&h, &hot, 1000);
    CHECK(h.zone[1].duty == 0);
    heating_clear_faults(&h);
    heating_update(&h, &hot, 1000);
    CHECK(h.zone[1].duty > 0);

    /* Broken sensor. */
    heat_inputs_t broken = inputs(25.0f, 0.0f);
    broken.pad_c[2] = -40.0f;
    faults = heating_update(&h, &broken, 1000);
    CHECK(faults & (1u << 2));
    CHECK(h.zone[2].fault == HEAT_FAULT_SENSOR);

    /* Low battery caps power, critical battery stops it. */
    heating_init(&h);
    heating_set_level(&h, 5);
    heat_inputs_t low = inputs(20.0f, 0.0f);
    low.battery_pct = 12;
    heating_update(&h, &low, 1000);
    CHECK(h.zone[0].duty <= HEAT_LOW_BATTERY_DUTY);
    low.battery_pct = 4;
    heating_update(&h, &low, 1000);
    CHECK(h.zone[0].duty == 0);

    /* Pad that never warms while driven hard: no-rise fault after a minute. */
    heating_init(&h);
    heating_set_level(&h, 5);
    heat_inputs_t stuck = inputs(20.0f, 0.0f);
    uint8_t any = 0;
    for (int s = 0; s < 70; s++) {
        any |= heating_update(&h, &stuck, 1000);
    }
    CHECK(any & 1u);
    CHECK(h.zone[0].fault == HEAT_FAULT_NO_RISE);
}
