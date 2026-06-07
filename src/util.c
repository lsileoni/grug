#include <stddef.h>

#include "util.h"

#if defined(_WIN32)
#include <windows.h>
long long timeNowMs(void)
{
    return (long long)GetTickCount64();
}
#else
#include <sys/time.h>
long long timeNowMs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif
