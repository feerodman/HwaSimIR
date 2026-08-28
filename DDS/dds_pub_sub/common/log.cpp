#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>

unsigned int log_smy_mode_show = LOG_SMY_MODE_SHOW_DEFAULT;
unsigned int log_smy_mode_log  = LOG_SMY_MODE_LOG_DEFAULT;
unsigned int log_smy_mode_err  = LOG_SMY_MODE_ERR_DEFAULT;
unsigned int log_smy_mode_info = LOG_SMY_MODE_INFO_DEFAULT;
unsigned int log_smy_mode_warn = LOG_SMY_MODE_WARN_DEFAULT;

unsigned int log_smy_modeb_show = LOG_SMY_MODEB_SHOW_DEFAULT;
unsigned int log_smy_modeb_log  = LOG_SMY_MODEB_LOG_DEFAULT;
unsigned int log_smy_modeb_err  = LOG_SMY_MODEB_ERR_DEFAULT;
unsigned int log_smy_modeb_info = LOG_SMY_MODEB_INFO_DEFAULT;
unsigned int log_smy_modeb_warn = LOG_SMY_MODEB_WARN_DEFAULT;

char               log_smy_buf_head[ LOG_SMY_BUF_SIZE ];
char               log_smy_buf[ LOG_SMY_BUF_SIZE ];
FILE              *log_smy_fp[ LOG_SMY_MODULE_MAX ]        = { 0 };
unsigned long long log_smy_file_size[ LOG_SMY_MODULE_MAX ] = { 0 };

// static long long getms()
//{
//     struct timespec time1 = {0, 0};
//     clock_gettime(1, &time1);
//     long long c = time1.tv_sec;
//     c = c*1000000000+time1.tv_nsec;
//     return c/1000000;
// }
long long getus()
{
    struct timespec time1 = { 0, 0 };
    clock_gettime( 1, &time1 );
    long long c = time1.tv_sec;
    c           = c * 1000000000 + time1.tv_nsec;
    return c / 1000;
}

// setSystemTime(2022, 4,12, 11, 19, 33);

// pid_t pid = getpid();
// struct sched_param param;
// param.sched_priority = sched_get_priority_max(SCHED_RR);
// sched_setscheduler(pid, SCHED_RR, &param);

#ifdef WIN32

#else
pthread_mutex_t log_smy_mutex;
#endif

int log_smy_mutex_available = 0;

int log_smy_lock_init()
{
#ifdef WIN32

#else

    if ( pthread_mutex_init( &log_smy_mutex, NULL ) != 0 )
    {
        log_smy_mutex_available = 0;
        return -1;
    }
    log_smy_mutex_available = 1;
#endif
    return 0;
}

void log_smy_lock_deinit()
{

#ifdef WIN32

#else
    if ( log_smy_mutex_available )
    {
        pthread_mutex_destroy( &log_smy_mutex );
        log_smy_mutex_available = 0;
    }
#endif
}

void log_smy_lock()
{
#ifdef WIN32

#else
    if ( !log_smy_mutex_available )
        log_smy_lock_init();
    if ( log_smy_mutex_available )
        pthread_mutex_lock( &log_smy_mutex );

#endif
}

void log_smy_unlock()
{
#ifdef WIN32

#else
    if ( log_smy_mutex_available )
        pthread_mutex_unlock( &log_smy_mutex );

#endif
}

void timeSTr( char *buf )
{
}

FILE *log_smy_file_open( int module )
{
    if ( access( LOG_SMY_DIR, F_OK ) )
    {
#ifdef WIN32
        mkdir( LOG_SMY_DIR );

#else
        mkdir( LOG_SMY_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
#endif
    }

    char buf[ 128 ];
    sprintf( buf, "%s/log.%d.txt", LOG_SMY_DIR, module );
    FILE *fp = fopen( buf, "a+" );
    if ( fp == NULL )
    {
        printf( "log_smy_file_open err, fopen failed, filename=%s\n", buf );
        return NULL;
    }
    fseek( fp, 0, SEEK_END );
    log_smy_file_size[ module ] = ftell( fp );

    // printf("log size=%lld,maxSize=%d\n", log_smy_file_size[module],FILE_MAX_SIZE);

    if ( log_smy_file_size[ module ] >= FILE_MAX_SIZE )
    {
        fclose( fp );

        char buf1[ 128 ];
        for ( int i = 0; i < FILE_COUNT_SIZE - 1; i++ )
        {
            sprintf( buf, "%s/log.%d.%d.txt", LOG_SMY_DIR, FILE_COUNT_SIZE - i - 1, module );
            sprintf( buf1, "%s/log.%d.%d.txt", LOG_SMY_DIR, FILE_COUNT_SIZE - i, module );
            rename( buf, buf1 );
            // printf("log rename, %s--->>%s\n", buf,buf1);
        }
        // log.0.txt---->log.1.0.txt
        sprintf( buf, "%s/log.%d.txt", LOG_SMY_DIR, module );
        sprintf( buf1, "%s/log.%d.%d.txt", LOG_SMY_DIR, 1, module );
        rename( buf, buf1 );
        // printf("log rename, %s--->>%s\n", buf,buf1);

        sprintf( buf, "%s/log.%d.txt", LOG_SMY_DIR, module );
        fp = fopen( buf, "a+" );
        if ( fp == NULL )
        {
            printf( "log_smy_file_open err, fopen failed, filename=%s\n", buf );
            return NULL;
        }
        fseek( fp, 0, SEEK_END );
        log_smy_file_size[ module ] = ftell( fp );
    }
    return fp;
}

void log_smy_f( int         module,
                const char *typestr,
                const char *file,
                int         line,
                const char *func,
                int         mode,
                char       *head,
                char       *buf )
{
    //	int ret;
    int pos = 0;

    head[ 0 ] = 0;

    pos += snprintf( head + pos, LOG_SMY_BUF_SIZE - pos, "%s", typestr );

    if ( mode & LOG_SMY_MODE_TIME )
    {
        time_t     tim = time( NULL );
        struct tm *timeinfo;
        timeinfo = localtime( &tim );
        if ( timeinfo == NULL )
            return;
        pos += snprintf( head + pos,
                         LOG_SMY_BUF_SIZE - pos,
                         "%04d%02d%02dT%02d%02d%02d:",
                         1900 + timeinfo->tm_year,
                         1 + timeinfo->tm_mon,
                         timeinfo->tm_mday,
                         timeinfo->tm_hour,
                         timeinfo->tm_min,
                         timeinfo->tm_sec );
    }

    if ( mode & LOG_SMY_MODE_RUNUS )
        pos += snprintf( head + pos, LOG_SMY_BUF_SIZE - pos, "%16lld:", getus() );

    if ( mode & LOG_SMY_MODE_LINE )
        pos += snprintf( head + pos, LOG_SMY_BUF_SIZE - pos, "%04d:", line );

    if ( mode & LOG_SMY_MODE_FILE )
    {
        for ( int i = strlen( file ) - 1; i >= 0; i-- )
        {
            if ( ( file[ i ] == '/' ) || file[ i ] == '\\' )
            {
                file = &file[ i + 1 ];
                break;
            }
        }

        pos += snprintf( head + pos, LOG_SMY_BUF_SIZE - pos, "%s:", file );
    }

    if ( mode & LOG_SMY_MODE_FUNC )
        pos += snprintf( head + pos, LOG_SMY_BUF_SIZE - pos, "%s():", func );

    if ( mode & LOG_SMY_MODE_PRINT )
    {
        fprintf( stdout, "%s", head );
        fprintf( stdout, "%s", buf );
        fflush( stdout );
    }
    if ( mode & LOG_SMY_MODE_SAVE )
    {
        // if(log_smy_fp[module]==NULL) printf("log fp null\n");
        if ( log_smy_fp[ module ] )
        {
            if ( log_smy_file_size[ module ] >= FILE_MAX_SIZE )
            {
                FILE *p              = log_smy_fp[ module ];
                log_smy_fp[ module ] = NULL;
                fclose( p );
                log_smy_file_size[ module ] = 0;
            }
        }

        if ( !log_smy_fp[ module ] )
        {
            log_smy_fp[ module ] = log_smy_file_open( module );
        }

        if ( log_smy_fp[ module ] )
        {

            log_smy_file_size[ module ] += fprintf( log_smy_fp[ module ], "%s", head );
            log_smy_file_size[ module ] += fprintf( log_smy_fp[ module ], "%s", buf );
            if ( mode & LOG_SMY_MODE_FLUSH )
                fflush( log_smy_fp[ module ] );

            // printf("log write, cursize=%lld\n",log_smy_file_size[module]);
        }
    }
}
