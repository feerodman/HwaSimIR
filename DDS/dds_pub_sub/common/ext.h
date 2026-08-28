#ifndef EXT_H
#define EXT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <algorithm>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;


#define StringMaxLen    (1024*4)


std::string string_format(const char* format, ...);


string char2string(const char *p, int size);
int string2bytes(char *p, const string &s, unsigned int size);
string bin2HexString(const char *p, int size);
int hexString2Bin(char *buf, int max, const string &s);
void truncatTailingZeroes(std::string &tmps);

#define CLOCKS CLOCKS_PER_SEC
long long getms();
long long getus();

inline bool isSpace(char c)
{
    return isspace(c);
}
int smy_read(istream  &in, char *buf, int size);
void smy_unread(istream  &in, char *buf, int size);

#define  doOneOfNTimes(Start,N) for(static int doOneOfNTimesCur=(Start);((N)>0)&&(doOneOfNTimesCur++>=(N));doOneOfNTimesCur=0)

#define Define2String1(x) #x
#define Define2String(x) Define2String1(x)


#endif // EXT_H
