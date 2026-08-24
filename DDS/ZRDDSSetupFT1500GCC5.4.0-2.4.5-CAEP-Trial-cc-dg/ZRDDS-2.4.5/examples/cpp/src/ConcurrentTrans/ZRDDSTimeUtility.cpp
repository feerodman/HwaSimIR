#include "ZRDDSTimeUtility.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <thread>
#else
#include <unistd.h>
#endif

void ZRDDSTimeUtility::zrusleep(unsigned int us)
{
#ifdef _WIN32
    std::this_thread::sleep_for(std::chrono::microseconds(us));
#elif defined(__linux__)
    usleep(us);
#endif
}

void ZRDDSTimeUtility::zrusleepsync(unsigned int us)
{
    uint64_t startTime = gettimestamp();
    while (true)
    {
        uint64_t currentTime = gettimestamp();
        if (currentTime >= startTime + us)
        {
            break;
        }
    }
}

uint64_t ZRDDSTimeUtility::gettimestamp(clockid_t clkId)
{
#if defined(_WIN32)
    if (m_freq == 0)
    {
        ZRDDSTimeUtility::start();
    }
    LARGE_INTEGER curCount;
    QueryPerformanceCounter(&curCount);
    return uint64_t(curCount.QuadPart * 1000000 / m_freq);
#elif defined(__linux__)
    timespec curTime;
    clock_gettime(clkId, &curTime);
    return (unsigned long long)curTime.tv_sec * 1000000 + (unsigned long long)curTime.tv_nsec / 1000;
#endif
}

void ZRDDSTimeUtility::start()
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_freq = (uint64_t)freq.QuadPart;
#endif
}

uint64_t ZRDDSTimeUtility::m_freq = 0;
