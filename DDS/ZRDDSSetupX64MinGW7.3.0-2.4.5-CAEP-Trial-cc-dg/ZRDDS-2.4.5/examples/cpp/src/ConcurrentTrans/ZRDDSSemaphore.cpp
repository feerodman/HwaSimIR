#include "ZRDDSSemaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#pragma warning(disable: 4996)

ZRDDSSemaphore::ZRDDSSemaphore()
{
    m_hSemaphore = NULL;
}

int ZRDDSSemaphore::init(int initVal, int maxVal)
{
#if defined(_WIN32)
    this->m_hSemaphore = CreateSemaphore(NULL, initVal, maxVal, NULL);
    if (this->m_hSemaphore == NULL)
    {
        printf("CreateSemaphore failed(%d %s).", errno, strerror(errno));
        return -1;
    }
#elif defined(__linux__)
    this->m_hSemaphore = (sem_t*)malloc(sizeof(sem_t));
    int ret = sem_init(this->m_hSemaphore, 0, initVal);
    if (ret != 0)
    {
        printf("sem_init(%u) failed(%d %s).", initVal, errno, strerror(errno));
        return -1;
    }
#endif
    return 0;
}

int ZRDDSSemaphore::take()
{
#if defined(_WIN32)
    unsigned int ret = WaitForSingleObject(this->m_hSemaphore, 0xffffffff);
    if (ret != WAIT_OBJECT_0)
    {
        printf("WaitForSingleObject failed(%d %s).", errno, strerror(errno));
        return -1;
    }
#elif defined(__linux__)
    if (sem_wait(this->m_hSemaphore) == -1)
    {
        printf("sem_wait failed(%d %s).", errno, strerror(errno));
        return -1;
    }
#endif
    return 0;
}

int ZRDDSSemaphore::post()
{
#if defined(_WIN32)
    ReleaseSemaphore(this->m_hSemaphore, 1, NULL);
#elif defined(__linux__)
    if (sem_post(this->m_hSemaphore) != 0)
    {
        printf("sem_post failed(%d %s).", errno, strerror(errno));
    }
#endif
    return 0;
}

void ZRDDSSemaphore::destory()
{
    if (this->m_hSemaphore == NULL)
    {
        return;
    }
#if defined(_WIN32)
    CloseHandle(this->m_hSemaphore);
#elif defined(__linux__) 
    sem_destroy(this->m_hSemaphore);
    free(this->m_hSemaphore);
#endif
    this->m_hSemaphore = NULL;
}
