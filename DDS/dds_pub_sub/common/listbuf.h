#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>



#define EmptysBufCountMax 1024

template <class T>
class ListBuf
{
public:

    std::mutex mtx;
    std::condition_variable cv;
    std::queue<T*> task_queue;


    ListBuf()
    {
    }

    T *get( int waitUs = 0 )
    {
        std::unique_lock<std::mutex> lck(mtx);
        T *p = NULL;
        if(waitUs==0)
        {
            if(!task_queue.empty())
            {
               p = task_queue.front();
               task_queue.pop();
            }
        }else if(waitUs>0)
        {
            cv.wait_for(lck,
                        std::chrono::microseconds(waitUs),
                        [this] { return !this->task_queue.empty(); });
            if(!task_queue.empty())
            {
               p = task_queue.front();
               task_queue.pop();
            }
        }else
        {
            cv.wait(lck,[this] { return !this->task_queue.empty(); });
            //if(!task_queue.empty())
            {
               p = task_queue.front();
               task_queue.pop();
            }
        }
        return p;
    }

    unsigned long long in_count=0;
    void put( T *p )
    {
        mtx.lock();
        task_queue.push(p);
        in_count++;
        cv.notify_one();
        mtx.unlock();
    }


    int count()
    {
        std::unique_lock<std::mutex> lck(mtx);
        return task_queue.size();
    }

    void clear_to_one()
    {
        T *p = NULL;
        mtx.lock();

        int size = task_queue.size();
        for(int i=0;i<size-1;i++)
        {
            p = task_queue.front();
            task_queue.pop();
            this->free(p);
        }
        mtx.unlock();
    }

    int trash()
    {
        mtx.lock();
        T *p = NULL;

        int size = task_queue.size();
        for(int i=0;i<size;i++)
        {
            p = task_queue.front();
            task_queue.pop();
            this->free(p);
        }
        mtx.unlock();
    }

    ////////////////////////////////////////////////////////////////////////////
    /// \brief emptys

    static std::list<T *> emptys;
    static std::mutex     emptys_lock;

    static T *alloc()
    {
        T *p = NULL;

        // p =  new T();
        // return p;

        emptys_lock.lock();

        if ( !emptys.empty() )
        {
            p = emptys.front();
            emptys.pop_front();
        }
        emptys_lock.unlock();

        while ( p == NULL )
        {
              posix_memalign( ( void ** )&p, 4096 /*alignment */, sizeof( T ) + 4096 );
           //p =  new T();
            if ( p != NULL )
            {
                static int totalCountEmpysAllocs = 0;

                int emcount = 0;
                emptys_lock.lock();
                emcount = emptys.size();
                emptys_lock.unlock();

                totalCountEmpysAllocs++;
                if ( ( totalCountEmpysAllocs % 100 ) == 0 )
                {
                    if ( totalCountEmpysAllocs > 0 )
                    {
                        logerr( "emptys cur=%d, alloc new buf,total=%d\n", emcount, ++totalCountEmpysAllocs );
                    }
                }
                break;
            }
            else
                usleep( 2000 );
        }
        return p;
    }

    static void free( T *p )
    {
        // delete p;
        // return;
        emptys_lock.lock();
        if ( emptys.size() > EmptysBufCountMax )
        {
            std::free(p);
           // delete p;
        }
        else
        {

            emptys.push_back( p );
        }
        emptys_lock.unlock();
    }

    static int freeCount( )
    {
        return emptys.size();
    }


};
template <class T>
std::list<T *> ListBuf<T>::emptys;
template <class T>
std::mutex ListBuf<T>::emptys_lock;



#endif // WORKQUEUE_H
