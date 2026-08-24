#ifndef ZRDDSSemaphore_h__
#define ZRDDSSemaphore_h__

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <semaphore.h>
#include <pthread.h>
#endif

/**
 * @class   ZRDDSSemaphore
 *
 * @brief   信号量。
 */

class ZRDDSSemaphore
{
public:

    /**
     * @fn  ZRDDSSemaphore::ZRDDSSemaphore();
     *
     * @brief   默认构造函数。
     */

    ZRDDSSemaphore();

    /**
     * @fn  int ZRDDSSemaphore::init(int initVal, int maxVal);
     *
     * @brief   初始化信号量。
     *
     * @param   initVal 信号量初始值。
     * @param   maxVal  信号量最大值。
     *
     * @return  0表示成功，小于0表示失败。
     */

    int init(int initVal, int maxVal);

    /**
     * @fn  void ZRDDSSemaphore::destory();
     *
     * @brief   回收资源。
     */

    void destory();

    /**
     * @fn  int ZRDDSSemaphore::take();
     *
     * @brief   等待信号量。
     *
     * @return  0表示成功，小于0表示失败。
     */

    int take();

    /**
     * @fn  int ZRDDSSemaphore::post();
     *
     * @brief   释放信号量。
     *
     * @return  0表示成功，小于0表示失败。
     */

    int post();
private:
#if defined(_WIN32)
    /** @brief	信号量句柄。 */
    HANDLE m_hSemaphore;
#elif defined(__linux__)
    /** @brief	linux下信号量的表示方法。 */
    sem_t* m_hSemaphore;
#endif
};

#endif // ZRDDSSemaphore_h__
