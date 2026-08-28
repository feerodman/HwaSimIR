#ifndef SmyExp_H
#define SmyExp_H

#include <string>
#include <stdarg.h>
#include <stdio.h>
#include "ext.h"

using namespace std;

namespace SmyExp
{
    class SmyExp
    {
    public:
        string what="[SmyExp";
        const string& toString() const
        {
            return what;
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXP_CLASS_BLOCK_COMMON(className)   \
    {\
        if(!file)file = "";\
        if(!func) func = "";\
        what += string_format("::%s]->[%s:%s:%4d]:",className, file,func, line);\
        char buff[1024] = {0};\
        va_list args;\
        va_start(args, fmt);\
        vsnprintf(buff, sizeof(buff), fmt, args);\
        va_end(args);\
        std::string str(buff);\
    \
        what += str;\
    }
#define DEFINE_CLASS_INSTRUCTION_FUNCTION(className)    \
    class className: public SmyExp\
    {\
    public:\
        className(const char *file, const int line, const char *func, const char *fmt, ...)\
        {\
            EXP_CLASS_BLOCK_COMMON(__func__);\
        }\
    };\

    //断言
    DEFINE_CLASS_INSTRUCTION_FUNCTION(AssertErr)
    #define Assert(x)   do{ if(!(x)) throw SmyExp::AssertErr(__FILE__, __LINE__,__func__,"%s","Assert("#x") err."); }while(0)
    #define AssertInfo(x,fmt,...)   do{ if(!(x)) throw SmyExp::AssertErr(__FILE__, __LINE__,__func__,"AssertInfo("#x") err." fmt,##__VA_ARGS__); }while(0)

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // common---------------------------------------------

    //时间---------------------------------------------
    //超时
    DEFINE_CLASS_INSTRUCTION_FUNCTION(TimeOut)
    #define TimeOut(fmt, ...) TimeOut(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

    //文件---------------------------------------------
    //文件打开失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(FileOpen)
    #define FileOpen(fmt, ...) FileOpen(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //读取数据失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(FileRead)
    #define FileRead(fmt, ...) FileRead(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //文件到达结尾
    DEFINE_CLASS_INSTRUCTION_FUNCTION(FileEof)
    #define FileEof(fmt, ...) FileEof(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //文件写入失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(FileWrite)
    #define FileWrite(fmt, ...) FileWrite(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

    //函数---------------------------------------------
    //参数为空指针
    DEFINE_CLASS_INSTRUCTION_FUNCTION(ParaNull)
    #define ParaNull(fmt, ...) ParaNull(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //参数格式非法
    DEFINE_CLASS_INSTRUCTION_FUNCTION(ParaFormat)
    #define ParaFormat(fmt, ...) ParaFormat(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //参数值非法
    DEFINE_CLASS_INSTRUCTION_FUNCTION(ParaValue)
    #define ParaValue(fmt, ...) ParaValue(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //参数数据长度非法
    DEFINE_CLASS_INSTRUCTION_FUNCTION(ParaDataSize)
    #define ParaDataSize(fmt, ...) ParaDataSize(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)


    //udp/tcp-------------------------------------------

    //socket创建失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetSocket)
    #define NetSocket(fmt, ...) NetSocket(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //绑定失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetBind)
    #define NetBind(fmt, ...) NetBind(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //发送失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetSend)
    #define NetSend(fmt, ...) NetSend(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //监听失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetListen)
    #define NetListen(fmt, ...) NetListen(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //远端关闭
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetRemoteClose)
    #define NetRemoteClose(fmt, ...) NetRemoteClose(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //连接失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetLink)
    #define NetLink(fmt, ...) NetLink(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //ip地址非法
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetIp)
    #define NetIp(fmt, ...) NetIp(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //端口号非法
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetPort)
    #define NetPort(fmt, ...) NetPort(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //sockOpt 失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetSockOpt)
    #define NetSockOpt(fmt, ...) NetSockOpt(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //读写超时.

    DEFINE_CLASS_INSTRUCTION_FUNCTION(NetTimeout)
    #define NetTimeout(fmt, ...) NetTimeout(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)


    //缓冲,内存,数组-------------------------------------------
    //new 失败
    DEFINE_CLASS_INSTRUCTION_FUNCTION(BufNew)
    #define BufNew(fmt, ...) BufNew(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //数组越界
    DEFINE_CLASS_INSTRUCTION_FUNCTION(BufOverflow)
    #define BufOverflow(fmt, ...) BufOverflow(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)


    //进程线程.-------------------------------------------
    //线程未运行
    DEFINE_CLASS_INSTRUCTION_FUNCTION(ThreadNotRun)
    #define ThreadNotRun(fmt, ...) ThreadNotRun(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //类型-------------------------------------------
    //未知类型
    DEFINE_CLASS_INSTRUCTION_FUNCTION(TypeUnknow)
    #define TypeUnknow(fmt, ...) TypeUnknow(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)
    //类型不符合
    DEFINE_CLASS_INSTRUCTION_FUNCTION(TypeUnmatch)
    #define TypeUnmatch(fmt, ...) TypeUnmatch(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    DEFINE_CLASS_INSTRUCTION_FUNCTION(Express)
    #define Express(fmt, ...) Express(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)




DEFINE_CLASS_INSTRUCTION_FUNCTION(OperatorErrNotObject)
#define OperatorErrNotObject(fmt, ...) OperatorErrNotObject(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(SeralizeErr)
#define SeralizeErr(fmt, ...) SeralizeErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(ArrayPosErr)
#define ArrayPosErr(fmt, ...) ArrayPosErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(ValueUnknowErr)
#define ValueUnknowErr(fmt, ...) ValueUnknowErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(OperatorErrType)
#define OperatorErrType(fmt, ...) OperatorErrType(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)




DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserStringErr)
#define PasserStringErr(fmt, ...) PasserStringErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserElementsErr)
#define PasserElementsErr(fmt, ...) PasserElementsErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserEleErr)
#define PasserEleErr(fmt, ...) PasserEleErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(IteratorErr)
#define IteratorErr(fmt, ...) IteratorErr(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)



DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserEleErrType)
#define PasserEleErrType(fmt, ...) PasserEleErrType(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserEleErrOverite)
#define PasserEleErrOverite(fmt, ...) PasserEleErrOverite(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)

DEFINE_CLASS_INSTRUCTION_FUNCTION(PasserEleErrUnMatch)
#define PasserEleErrUnMatch(fmt, ...) PasserEleErrUnMatch(__FILE__, __LINE__,__func__,fmt, ##__VA_ARGS__)



}

#endif // SmyExp_H
