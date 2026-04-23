// time package runtime
// I think this is linux only
// Almost sure

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static double ccpl_time_unix(void) {
    return (double)time(NULL);
}

static double ccpl_time_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
}

static double ccpl_time_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((double)ts.tv_sec * 1000.0) + ((double)ts.tv_nsec / 1000000.0);
}

static double ccpl_time_sleep_ms(double ms) {
    if (ms <= 0.0) return 0.0;
    usleep((useconds_t)(ms * 1000.0));
    return 0.0;
}
