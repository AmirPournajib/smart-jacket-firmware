/* Minimal test helpers: no framework to install, runs anywhere a C compiler does. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        g_checks++;                                                                    \
        if (!(cond)) {                                                                 \
            g_failures++;                                                              \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
        }                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(((a) - (b)) <= (tol) && ((b) - (a)) <= (tol))

#define RUN(fn)                                                                        \
    do {                                                                               \
        printf("%s\n", #fn);                                                           \
        fn();                                                                          \
    } while (0)

void test_imu(void);
void test_model(void);
void test_vitals(void);
void test_heating(void);
void test_voice(void);
void test_protocol(void);
void test_app(void);

#endif
