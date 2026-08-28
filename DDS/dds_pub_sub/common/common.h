#ifndef COMMON_H
#define COMMON_H

#include <sys/timeb.h>
#include <time.h>
#include <dirent.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
using namespace std;
#include <string.h>

/*
获取文件大小
*/
long long get_fileSize( const char *filename );

//设置板卡系统日期函数
int setSystemTime( short date, unsigned int us100 );
int setSystemTime( struct tm *t );
int setSystemTime( int year, int month, int day, int hour, int min, int sec );

long long   getms();
unsigned long long get_time_us();
unsigned long long get_time_ms();
std::string string_format( const char *format, ... );

#define StringMaxLen 1024

string char2string( const char *p, int size );
int    string2bytes( char *p, const string &s, unsigned int size );
string bin2HexString( const char *p, int size );
int    hexString2Bin( char *buf, int max, const string &s );

//获取从2000年至今的天数, //获取当天时间的0.1ms数
void           getDaysFrom2000_100usOfDay( unsigned short *days, unsigned int *us100 );
struct timeval parseDateTime_2000( short date, unsigned int us100 );
//获取时间戳字符串。
string getTimeStr();

#define EndianChange( x )                                \
    do                                                   \
    {                                                    \
        unsigned char *p    = ( unsigned char    *)&( x ); \
        int            size = sizeof( x );               \
        for ( int i = 0; i < size / 2; i++ )             \
        {                                                \
            unsigned char c   = p[ i ];                  \
            p[ i ]            = p[ size - i - 1 ];       \
            p[ size - i - 1 ] = c;                       \
        }                                                \
    } while ( 0 )

inline int min( int a, int b )
{
    return a > b ? b : a;
}

//获取文件系统信息：
// mnt指定文件系统
// ret：已用空间百分比*100
// blocksK：总计空间 单位KB
// availK ：可用空间 单位KB

int diskFreeK( const char *mnt, unsigned int *blocksK, unsigned int *availK );
#endif  // COMMON_H
