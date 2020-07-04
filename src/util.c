#include "util.h"
#include <stdio.h>
#include <time.h>

uint64_t get_time(void)
{
    struct timespec ts;
    int ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (ret < 0) {
        perror("Util: clock_gettime");
        return 0;
    }
    return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}
