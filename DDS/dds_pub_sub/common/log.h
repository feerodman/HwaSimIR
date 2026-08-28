#ifndef __LOG_H__
#define __LOG_H__

//#define printf_info    printf
//#define printf_err    printf
//#define printf_warn    printf

//#define loginfo    printf
//#define logerr    printf
//#define logwarn    printf

#define LOG_SMY_MODE_FILE  ( 1 << 0 )
#define LOG_SMY_MODE_FUNC  ( 1 << 1 )
#define LOG_SMY_MODE_LINE  ( 1 << 2 )
#define LOG_SMY_MODE_TIME  ( 1 << 3 )
#define LOG_SMY_MODE_RUNUS ( 1 << 4 )
#define LOG_SMY_MODE_LOCK  ( 1 << 5 )

#define LOG_SMY_MODE_PRINT ( 1 << 8 )
#define LOG_SMY_MODE_SAVE  ( 1 << 9 )
#define LOG_SMY_MODE_FLUSH ( 1 << 10 )

#define LOG_SMY_MODULE_MAX 8
#define LOG_SMY_MODULE_0   0
#define LOG_SMY_MODULE_1   1
#define LOG_SMY_MODULE_2   2
#define LOG_SMY_MODULE_3   3
#define LOG_SMY_MODULE_4   4
#define LOG_SMY_MODULE_5   5
#define LOG_SMY_MODULE_6   6
#define LOG_SMY_MODULE_7   7

#define FILE_MAX_SIZE   ( 1024 * 1024 * 100 )  // 100M
#define FILE_COUNT_SIZE ( 5 )                  // fileSize*5
#define LOG_SMY_DIR     "./log"

#define LOG_SMY_BUF_SIZE ( 1024 )
void log_smy_f( int         module,
                const char *typestr,
                const char *file,
                int         line,
                const char *func,
                int         mode,
                char       *head,
                char       *buf );
int  log_smy_lock_init();
void log_smy_lock_deinit();
void log_smy_lock();
void log_smy_unlock();

#define log_smy_nop( module, mode, fmt, ... ) \
    do                                        \
    {                                         \
        ;                                     \
    } while ( 0 )
#define log_smy_nopb( fmt, buf, bufend, size ) \
    do                                         \
    {                                          \
        ;                                      \
    } while ( 0 )

// example:
//#define lognop(fmt,...) log_smy_nop(0,0,fmt,##__VA_ARGS__)
//#define lognopb(fmt,buf,bufend,size) log_smy_nopb(fmt,buf,bufend,size)

#include <stdio.h>

#define log_smy_m( module, mode, typestr, fmt, ... )                           \
    do                                                                         \
    {                                                                          \
        char log_smy_buf_head[ LOG_SMY_BUF_SIZE ];                             \
        char log_smy_buf[ LOG_SMY_BUF_SIZE ];                                  \
        if ( mode & ( LOG_SMY_MODE_SAVE | LOG_SMY_MODE_PRINT ) )               \
        {                                                                      \
            if ( ( mode )&LOG_SMY_MODE_LOCK )                                  \
                log_smy_lock();                                                \
            snprintf( log_smy_buf, LOG_SMY_BUF_SIZE, ( fmt ), ##__VA_ARGS__ ); \
            log_smy_f( ( module ),                                             \
                       ( char * )( typestr ),                                  \
                       ( char * )__FILE__,                                     \
                       __LINE__,                                               \
                       __FUNCTION__,                                           \
                       ( mode ),                                               \
                       log_smy_buf_head,                                       \
                       log_smy_buf );                                          \
            if ( ( mode )&LOG_SMY_MODE_LOCK )                                  \
                log_smy_unlock();                                              \
        }                                                                      \
    } while ( 0 )

#define log_smy_b( module, mode, typestr, fmt, buf, bufend, size )                                   \
    do                                                                                               \
    {                                                                                                \
        char log_smy_buf_head[ LOG_SMY_BUF_SIZE ];                                                   \
        char log_smy_buf[ LOG_SMY_BUF_SIZE ];                                                        \
        if ( mode & ( LOG_SMY_MODE_SAVE | LOG_SMY_MODE_PRINT ) )                                     \
        {                                                                                            \
            if ( ( mode )&LOG_SMY_MODE_LOCK )                                                        \
                log_smy_lock();                                                                      \
            int pos = 0;                                                                             \
            for ( int i = 0; i < ( ( int )size ); i++ )                                              \
                pos += snprintf( log_smy_buf + pos, LOG_SMY_BUF_SIZE - pos, ( fmt ), ( buf )[ i ] ); \
            pos += snprintf( log_smy_buf + pos, LOG_SMY_BUF_SIZE - pos, "%s", ( bufend ) );          \
            if ( pos < LOG_SMY_BUF_SIZE )                                                            \
                log_smy_buf[ pos ] = 0;                                                              \
            else                                                                                     \
                log_smy_buf[ LOG_SMY_BUF_SIZE - 1 ] = 0;                                             \
            log_smy_f( ( module ),                                                                   \
                       ( char * )( typestr ),                                                        \
                       ( char * )__FILE__,                                                           \
                       __LINE__,                                                                     \
                       __FUNCTION__,                                                                 \
                       ( mode ),                                                                     \
                       log_smy_buf_head,                                                             \
                       log_smy_buf );                                                                \
            if ( ( mode )&LOG_SMY_MODE_LOCK )                                                        \
                log_smy_unlock();                                                                    \
        }                                                                                            \
    } while ( 0 )

#define LOG_SMY_MODE_SHOW_DEFAULT LOG_SMY_MODE_PRINT

#define LOG_SMY_MODE_LOG_DEFAULT                                                                       \
    LOG_SMY_MODE_FILE | LOG_SMY_MODE_LINE | LOG_SMY_MODE_PRINT | LOG_SMY_MODE_FILE | LOG_SMY_MODE_TIME \
        | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODE_ERR_DEFAULT                                                                       \
    LOG_SMY_MODE_FILE | LOG_SMY_MODE_LINE | LOG_SMY_MODE_PRINT | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_TIME \
        | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODE_INFO_DEFAULT                                                                      \
    LOG_SMY_MODE_PRINT | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_TIME \
        | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODE_WARN_DEFAULT                                                                      \
    LOG_SMY_MODE_FILE | LOG_SMY_MODE_LINE | LOG_SMY_MODE_PRINT | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_TIME \
        | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_FLUSH
extern unsigned int log_smy_mode_show;
extern unsigned int log_smy_mode_log;
extern unsigned int log_smy_mode_err;
extern unsigned int log_smy_mode_info;
extern unsigned int log_smy_mode_warn;

#define LOG_SMY_MODEB_SHOW_DEFAULT LOG_SMY_MODE_PRINT

#define LOG_SMY_MODEB_LOG_DEFAULT  LOG_SMY_MODE_PRINT | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODEB_ERR_DEFAULT  LOG_SMY_MODE_PRINT | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODEB_INFO_DEFAULT LOG_SMY_MODE_PRINT | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_FLUSH
#define LOG_SMY_MODEB_WARN_DEFAULT LOG_SMY_MODE_PRINT | LOG_SMY_MODE_LOCK | LOG_SMY_MODE_SAVE | LOG_SMY_MODE_FLUSH

extern unsigned int log_smy_modeb_show;
extern unsigned int log_smy_modeb_log;
extern unsigned int log_smy_modeb_show;
extern unsigned int log_smy_modeb_err;
extern unsigned int log_smy_modeb_info;
extern unsigned int log_smy_modeb_warn;

#define logshow( fmt, ... )  log_smy_m( LOG_SMY_MODULE_0, log_smy_mode_show, "", fmt, ##__VA_ARGS__ )
#define logerr( fmt, ... )   log_smy_m( LOG_SMY_MODULE_0, log_smy_mode_err, "err  :", fmt, ##__VA_ARGS__ )
#define loginfo( fmt, ... )  log_smy_m( LOG_SMY_MODULE_0, log_smy_mode_info, "info :", fmt, ##__VA_ARGS__ )
#define logwarn( fmt, ... )  log_smy_m( LOG_SMY_MODULE_0, log_smy_mode_warn, "warn :", fmt, ##__VA_ARGS__ )
#define lognop( fmt, ... )   log_smy_nop( 0, 0, fmt, ##__VA_ARGS__ )  //
#define logdebug( fmt, ... ) log_smy_m( LOG_SMY_MODULE_0, log_smy_mode_err, "debug:", fmt, ##__VA_ARGS__ )

#define logshowb( fmt, buf, bufend, size )  log_smy_b( LOG_SMY_MODULE_0, log_smy_modeb_show, "", fmt, buf, bufend, size )
#define logerrb( fmt, buf, bufend, size )   log_smy_b( LOG_SMY_MODULE_0, log_smy_modeb_err, "", fmt, buf, bufend, size )
#define loginfob( fmt, buf, bufend, size )  log_smy_b( LOG_SMY_MODULE_0, log_smy_modeb_info, "", fmt, buf, bufend, size )
#define logwarnb( fmt, buf, bufend, size )  log_smy_b( LOG_SMY_MODULE_0, log_smy_modeb_warn, "", fmt, buf, bufend, size )
#define lognopb( fmt, buf, bufend, size )   log_smy_nopb( fmt, buf, bufend, size )  //
#define logdebugb( fmt, buf, bufend, size ) log_smy_b( LOG_SMY_MODULE_0, log_smy_modeb_err, "", fmt, buf, bufend, size )
long long getus();

#endif
