// dt package runtime
// This just converts time to various formats
#include <sys/time.h>
#include <time.h>

static double ccpl_dt_unix(void) {
    return (double)time(NULL);
}

static double ccpl_dt_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
}

static char _ccpl_dt_buf1[128];
static char _ccpl_dt_buf2[128];

static const char *ccpl_dt_now_iso(void) {
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(_ccpl_dt_buf1, sizeof(_ccpl_dt_buf1), "%Y-%m-%dT%H:%M:%S", &tmv);
    return _ccpl_dt_buf1;
}

static const char *ccpl_dt_format_unix(double ts) {
    time_t t = (time_t)ts;
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(_ccpl_dt_buf2, sizeof(_ccpl_dt_buf2), "%Y-%m-%d %H:%M:%S", &tmv);
    return _ccpl_dt_buf2;
}
