#include "test.h"

int g_checks = 0;
int g_failures = 0;

int main(void)
{
    RUN(test_imu);
    RUN(test_model);
    RUN(test_vitals);
    RUN(test_heating);
    RUN(test_voice);
    RUN(test_protocol);
    RUN(test_app);
    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
