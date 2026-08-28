#ifndef ZRDDSTimeUtility_h__
#define ZRDDSTimeUtility_h__

#include <stdint.h>
#ifdef _WIN32
typedef int clockid_t;
#define CLOCK_REALTIME 0
#else
#include <time.h>
#endif

class ZRDDSTimeUtility
{
public:

    /**
     * @fn  static ZRDDSTimeUtility::usleep(unsigned int us);
     *
     * @brief   微秒延时。
     *
     * @param   us  需要延时的时间。
     */

    static void zrusleep(unsigned int us);

    /**
     * @fn  static void zrusleepsync(unsigned int us);
     *
     * @brief   忙等延时。
     *
     * @param   us  微秒。
     */

    static void zrusleepsync(unsigned int us);

    /**
     * @fn  static void ZRDDSTimeUtility::start();
     *
     * @brief   开始计时。
     */

    static void start();

    /**
     * @fn  static uint64_t gettimestamp(clockid_t clkId);
     *
     * @brief   从指定时钟源获取当前时间戳。
     *
     * @param   clkId   时间源标识，仅在Linux上有效。
     *
     * @return  当前时戳，us。
     */

    static uint64_t gettimestamp(clockid_t clkId = CLOCK_REALTIME);
private:
    /** @brief   时钟频率。 */
    static uint64_t m_freq;
};
#endif // ZRDDSTimeUtility_h__
