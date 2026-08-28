#ifndef SIMPLETHREAD_H
#define SIMPLETHREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <ctype.h>

#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include <pthread.h>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

// 当程序运行前 running为1， stopped为0，
//当需要结束时，外部stop()， 内部将stoped设置为1，
//外部检查isRunning直到为0时删除该模块。

//使用过程。
// 1. 继承继承或创建该类实例。传入服务函数和调用实体（将哪个对象传入该线程，以供使用）。
// 2. 调用start函数启动线程。此时开始执行传入的服务函数。
// 3. 在服务函数中不断检查线程健康状态，如self->healthy(),
// 若为假，说明外部发起了停止该线程当命令。此时结束循环并释放需要当资源。返回状态码。服务函数进行return x；
// 4. 外部需要停止该线程时， 调用xx->stop(),通知线程进行结束运行， 若为假，
// 说明线程还未停止，正常时此时应该在处理释放资源。若为真，说明线程已经完全结束。如果线程服务
//   函数内部有自定义当休眠过程， 需自行编写处理唤醒操作， 否则线程将无法结束。
// 5. 若需线程自行结束， 那么外部等待线程可以使用isRunning（）函数， 为真表示线程还未结束， 为假表示线程已经完全结束。
// 6. 注：需自行处理过程中自行分配当资源。

//实现过程。
// 1. 实例创建时， 模块保存传入当服务函数和调用实体（将哪个对象传入该线程，以供使用）。此时running=false，stopped=true
// 2. 调用start()后， running=true，stopped=false, 线程检查healthy()直到为真时，循环运行。
// 3. 线程内部自行停止后， 执行return n， n为状态码，自行定义， 外部通过exitCode()， 查看该值。
// 4. 外部通过stop()停止后， running=false，stopped=false。外部通过返回值判断是否完全停止。
// 5. 线程检查healthy()此时为假， 跳出循环释放资源，执行return x。 之后 running=false，stopped=false。

typedef void *( *Pt_Func_t )( void * );

class SimpleThread
{
public:
    SimpleThread()
    {
        exitCode_     = 0;
        running = false;
        stopped = true;
        pid     = 0;
    }
    static void setRt()
    {
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_RR);

        if (sched_setscheduler(0, SCHED_RR, &param) == -1)
        {
            perror("sched_setscheduler");
            return;
        }


    }

    bool start()
    {
        if ( isRunning() ) return false;

        running = true;
        stopped = false;
        if ( !pthread_create( &pid, NULL, ( Pt_Func_t )SimpleThread::run_sth, this ) )
        {
            pthread_detach( pid );
            return true;
        }
        else
        {
            pid     = 0;
            running = false;
            stopped = true;
            return false;
        }
    }

    inline void join()
    {
        pthread_join( pid, NULL );
    }

    inline bool stop()
    {
        running = false;
        return stopped;
    }

    inline bool isRunning()
    {
        return running || !stopped;
    }

    inline bool healthy()
    {
        return running;
    }

    inline int exitCode()
    {
        return this->exitCode_;
    }

private:
    pthread_t     pid;
    volatile bool running;
    volatile bool stopped;
    int   exitCode_;

    virtual int run()=0;

    static void *run_sth( SimpleThread *sth )
    {

        sth->exitCode_ = sth->run();
        sth->stopped   = true;
        sth->running   = false;
        sth->pid       = 0;

        return NULL;
    }
};

#endif  // SIMPLETHREAD_H
