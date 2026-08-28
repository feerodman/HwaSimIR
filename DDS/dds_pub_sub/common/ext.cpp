#include "ext.h"




std::string string_format(const char* format, ...)
{
#if 1 // 最大长度限制：StringMaxLen - 1
    char buff[StringMaxLen] = {0};

    va_list args;
    va_start(args, format);
    vsnprintf(buff, sizeof(buff), format, args);
    va_end(args);

    std::string str(buff);
    return str;
#else // 无长度限制
    va_list args;
    va_start(args, format);
    int count = vsnprintf(NULL, 0, format, args); // 使用vsnprintf：warning C4996; 使用vsnprintf_s：无法自动计算长度
    va_end(args);

    va_start(args, format);
    char* buff = (char*)malloc(count * sizeof(wchar_t));
    if(buff!=NULL)
    {
        vsnprintf(buff, count, format, args);
        va_end(args);

        std::string str(buff, count);
        free(buff);
        return str;

    }

    return std::string("");
#endif
}


//将hex的string转化为bin。
inline unsigned char hexChar2Value(char c)
{
    if((c>='0')&&(c<='9')) return c-'0';
    if((c>='a')&&(c<='f')) return c-'a'+10;
    if((c>='A')&&(c<='F')) return c-'A'+10;
    return 0;
}
inline unsigned char hexChar2Value(char hi, char lo)
{
    return (hexChar2Value(hi)<<4) | hexChar2Value(lo);
}

int hexString2Bin(char *buf, int max, const string &s)
{
    const char *p = s.data();
    int len = s.length();
    len/=2;
    if(len>max) len = max;

    for(int i=0;i<len;i++)
    {
        buf[i] = hexChar2Value(p[i*2],p[i*2+1]);
    }

    return len;
}

//将char数组的bin数据转换为hex字符串。
string bin2HexString(const char *buf, int size)
{
    if(size<0) size=0;

    char *p = new char[size*2+1];
    if(p==NULL)
    {
        return "";
    }
    for(int i=0;i<size;i++)
    {
        sprintf(p+i*2, "%02X", (unsigned char)buf[i]);
    }
    p[size*2] = '\0';
    string s(p);
    delete[] p;

    return s;

}
//将char数组字符串取出size个字节作为字符串。
string char2string(const char *p, int size)
{
    if(size<0) size=0;
    if(size>=StringMaxLen) size=StringMaxLen-1;

    char bytes[StringMaxLen];
    memcpy(bytes, p, size);
    bytes[size]='\0';

    string s(bytes);
    return s;
}
int string2bytes(char *p, const string &s, unsigned int size)
{
    if(s.length()<size) size = s.length();
    memcpy(p,s.data(),size);
    return size;
}
string &trimAll(string &s)
{
    remove_if(s.begin(),s.end(),isSpace);
    return s;
}
string &toLower(string &s)
{
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}
string &toUpper(string &s)
{
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

int smy_read(istream  &in, char *buf, int size)
{
    int i=0;
    for(i=0;i<size;i++)
    {
        if(in.eof()) break;
        in.read(&buf[i],1);
    }
   // buf[i]='\0';
    //printf("smy_read:%s\n",buf);
    return i;
}

void smy_unread(istream  &in, char *buf, int size)
{
    for(int i=size-1;i>=0;i--)
    {
        in.putback(buf[i]);
    }
}
void truncatTailingZeroes(std::string &tmps)
{
    //删除尾部多余的0，如果尾部以点结束，也删除小数点
    const char *s = tmps.data();
    int len = tmps.length();
    for(int i=0;i<len;i++)
    {
        if(s[i]=='.')
        {
            int end=len-1;
            while(end>i)
            {
                if(s[end]!='0') break;
                end--;
            }
            if(end<len-1)
            {
                if(end==i)
                {
                    tmps = tmps.substr(0,end);
                }else
                {
                    tmps = tmps.substr(0,end+1);
                }
            }

        }
    }
    return;
//    if(tmps.find(".")>0)
//    {
//        size_t fp = tmps.rfind(".");
//        size_t f = tmps.rfind("0");
//        while (f > fp) {
//            if(f == string::npos) break;
//            tmps = tmps.erase(f);
//            f = tmps.rfind("0");
//        }
//        fp = tmps.rfind(".");
//        if(fp == tmps.size() - 1)
//        {
//            tmps = tmps.erase(fp);
//        }
//    }
}

#ifdef WIN32
#include <time.h>
long long getms()
{
    long long c = clock();
    return c*1000/CLOCKS_PER_SEC;
}
long long getus()
{
    long long c = clock();
    return c*1000000/CLOCKS_PER_SEC;
}
//long long getms()
//{
//	long long nMillSec = 0;
//	struct timeval tv;
//	gettimeofday(&tv,NULL);
//	nMillSec = (long long)tv.tv_sec * 1000;
//	nMillSec += tv.tv_usec / 1000;
//	return nMillSec;
//}
//long long getus()
//{
//	long long us = 0;
//	struct timeval tv;
//	gettimeofday(&tv,NULL);
//	us = (long long)tv.tv_sec *1000000;
//	us += tv.tv_usec;
//	return us;
//}

#else
long long getms()
{
    struct timespec time1 = {0, 0};
    //clock_gettime(CLOCK_MONOTONIC, &time1);
    //printf("CLOCK_MONOTONIC: %d, %d", time1.tv_sec, time1.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &time1);
    //printf("CLOCK_MONOTONIC: %d, %d", time1.tv_sec, time1.tv_nsec);
    long long c = time1.tv_sec;
	c = c*1000000000+time1.tv_nsec;
    return c/1000000;

   // long long c = clock();
    //return c*1000/CLOCKS_PER_SEC;
}
//long long getus()
//{
//    struct timespec time1 = {0, 0};
//    //clock_gettime(CLOCK_MONOTONIC, &time1);
//    //printf("CLOCK_MONOTONIC: %d, %d", time1.tv_sec, time1.tv_nsec);
//    clock_gettime(CLOCK_MONOTONIC, &time1);
//    //printf("CLOCK_MONOTONIC: %d, %d", time1.tv_sec, time1.tv_nsec);
//    long long c = time1.tv_sec;
//	c = c*1000000000+time1.tv_nsec;
//    return c/1000;


//    //long long c = clock();
//    //return c*1000000/CLOCKS_PER_SEC;
//}
#endif
