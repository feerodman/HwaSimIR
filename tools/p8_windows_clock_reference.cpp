// Independent Win32 QPC reference for cross-toolchain local time windows.
#include <windows.h>
#include <chrono>
#include <iostream>
#include <cmath>
int main() {
    LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);
    long long maxOutside=0;
    for(int i=0;i<1000;++i){
        QueryPerformanceCounter(&a);
        auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        QueryPerformanceCounter(&b);
        long long lo=static_cast<long long>(static_cast<long double>(a.QuadPart)*1000000000.L/f.QuadPart);
        long long hi=static_cast<long long>(static_cast<long double>(b.QuadPart)*1000000000.L/f.QuadPart);
        long long outside=ns<lo?lo-ns:(ns>hi?ns-hi:0);
        if(outside>maxOutside)maxOutside=outside;
    }
    std::cout<<"samples=1000 qpcFrequency="<<f.QuadPart<<" maxOutsideBracketNs="<<maxOutside
             <<" result="<<(maxOutside<=100000?"PASS":"FAIL")<<" toleranceNs=100000\n";
    return maxOutside<=100000?0:1;
}
