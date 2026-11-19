#include "sj_types.h"

const char *sj_activity_name(sj_activity_t a)
{
    switch (a) {
    case SJ_ACT_STILL: return "still";
    case SJ_ACT_WALK: return "walk";
    case SJ_ACT_RUN: return "run";
    default: return "?";
    }
}

const char *sj_zone_name(sj_zone_t z)
{
    switch (z) {
    case SJ_ZONE_CHEST: return "chest";
    case SJ_ZONE_BACK: return "back";
    case SJ_ZONE_COLLAR: return "collar";
    default: return "?";
    }
}
