#ifndef SMYJSON_H
#define SMYJSON_H

#include <string>
#include <list>
#include <map>
#include "smyexp.h"
#include <vector>

extern int testSmyJson();

using namespace std;

#include<iostream>
#include<sstream>
#include<string>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <map>
#include<list>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <math.h>





//class TcpStreamBuf : public std::streambuf {
//public:
//    enum { BUFSIZE = 1 << 5 };
//    int sd;

//    TcpStreamBuf(int sd)
//    {
//        this->sd = sd;
//    }


//protected:
//    // Buffered get
//    int underflow() override
//    {
//        auto n = ::recv(sd, buf, BUFSIZE, 0);
//        return n > 0 ? (setg(buf, buf, buf + n), *gptr()) : EOF;
//    }
//    // Unbuffered put
//    int overflow(int c) override
//    {
//        if (c == EOF) return 0;
//        char b = c;
//        return ::send(sd, &b, 1, 0) > 0 ? c : EOF;
//    }
//    std::streamsize xsputn(const char *s, std::streamsize n) override
//    {
//        auto x = ::send(sd, s, n, 0);
//        return x > 0 ? x : 0;
//    }
//    // flush
//    int sync() override
//    {
//        return 0;

//    }
//    //streamsize showmanyc() override { return 1; }

//private:
//    char buf[BUFSIZE];
//};

//class tcpstream : public std::iostream {
//public:
//    tcpstream(int sd) : std::iostream(&_buf) ,_buf(sd)
//    {

//    }
//private:
//    TcpStreamBuf _buf;
//};



using namespace std;
//int main()
//{
//    string str="i an a boy";
//    istringstream is(str);
//    string s;
//    while(is>>s)
//    {
//        cout<<s<<endl;
//    }

//}

static const string  typeStr[] = {"NUL", "NUMBER", "DOUBLE", "BOOL", "STRING", "ARRAY", "OBJECT", "BraceL", "BraceR", "BracketL", "BracketR", "Colon","Comma"};

static const double zero_double = 0.00000000000001;

class SmyJson
{
public:
//    // Types
//    enum  Type    { NUL, NUMBER, DOUBLE, BOOL, STRING, ARRAY, OBJECT   };
//    const string typeStr[]= { "NUL", "NUMBER", "DOUBLE", "BOOL", "STRING", "ARRAY", "OBJECT"    };

// Types
enum  Type    { NUL, NUMBER, DOUBLE, BOOL, STRING, ARRAY, OBJECT   ,
                //                {                }       [        ]         :      ,
                BraceL, BraceR, BracketL, BracketR, Colon,Comma     // used for elements.
              };



class Value
{
public:
    Type t;
    long long v_number;
    double v_double;
    string v_string;
    list<Value> v_list;
    map<string, Value> v_object;
    string key;

    /////////////////////////////////////////////////////////////////////////////////////////

public:
    bool exist(const string &name)
    {
        if(t==Type::OBJECT)
        {
            return v_object.count(name)>0;
        }else
        {
            return false;
        }
    }

    int size()
    {
        if(t==ARRAY)
        {
            return v_list.size();
        }else if(t==OBJECT)
        {
            return v_object.size();
        }else  if(t==NUL)
        {
            return 0;
        }else
        {
            return 1;
        }
    }

    class iterator
    {
        list<Value>::iterator v_list_it;
        map<string, Value>::iterator v_object_it;
        Type t=NUL;

        Value *container;

    public:

        iterator(Value *ic) : container(ic)
        {
            if(ic==NULL)
            {
                throw SmyExp::IteratorErr("iterator constructor err, parameter is NULL\n");
            }
            if(ic->t==ARRAY)        t = ARRAY;
            else if(ic->t==OBJECT)   t = OBJECT;
            else if(ic->t==NUL)     t = ARRAY;
            else
            {
                throw SmyExp::IteratorErr("iterator constructor err, type=%s\n",typeStr[ic->t].data());
            }
        }
        //            void erase()
        //            {
        //                if(t==ARRAY)
        //                {
        //                    container->v_list.erase(v_list_it);
        //                }else if(t==OBJECT)
        //                {
        //                    container->v_object.erase(v_object_it);
        //                }else
        //                {
        //                    throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
        //                }
        //            }

        void begin()
        {
            if(t==ARRAY)
            {
                v_list_it = container->v_list.begin();
            }else if(t==OBJECT)
            {
                v_object_it = container->v_object.begin();
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
        }

        void end()
        {
            if(t==ARRAY)
            {
                v_list_it = container->v_list.end();
            }else if(t==OBJECT)
            {
                v_object_it = container->v_object.end();
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
        }

        iterator& operator +=(int right)
        {
            if(t==ARRAY)
            {
                for(int i=0;i<right;i++) v_list_it++;
            }else if(t==OBJECT)
            {
                for(int i=0;i<right;i++) v_object_it++;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return *this;
        }
        iterator& operator -=(int right)
        {
            if(t==ARRAY)
            {
                for(int i=0;i<right;i++) v_list_it--;
            }else if(t==OBJECT)
            {
                for(int i=0;i<right;i++) v_object_it--;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return *this;
        }
        iterator& operator ++()
        {
            if(t==ARRAY)
            {
                ++v_list_it;
            }else if(t==OBJECT)
            {
                ++v_object_it;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return *this;
        }
        iterator& operator --()
        {
            if(t==ARRAY)
            {
                --v_list_it;
            }else if(t==OBJECT)
            {
                --v_object_it;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return *this;
        }

        Value& operator *()
        {
            if(t==ARRAY)
            {
                return *v_list_it;
            }else if(t==OBJECT)
            {
                return v_object_it->second;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
        }

        bool operator !=(iterator it)
        {
            if(t==ARRAY)
            {
                return this->v_list_it != it.v_list_it;
            }else if(t==OBJECT)
            {
                return this->v_object_it != it.v_object_it;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
        }
        bool operator ==(iterator it)
        {

            if(t==ARRAY)
            {
                return this->v_list_it == it.v_list_it;
            }else if(t==OBJECT)
            {
                return this->v_object_it == it.v_object_it;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
        }

        iterator operator +(int right)
        {
            iterator result = *this;
            if(t==ARRAY)
            {
                for(int i=0;i<right;i++) v_list_it++;
            }else if(t==OBJECT)
            {
                for(int i=0;i<right;i++) v_object_it++;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return result;
        }
        iterator operator -(int right)
        {
            iterator result = *this;
            if(t==ARRAY)
            {
                for(int i=0;i<right;i++) v_list_it--;
            }else if(t==OBJECT)
            {
                for(int i=0;i<right;i++) v_object_it--;
            }else
            {
                throw SmyExp::IteratorErr("iterator use err, constructor type=%s\n",typeStr[t].data());
            }
            return result;
        }

    };

    iterator begin()
    {
        iterator x(this);
        x.begin();
        return x;
    }

    iterator end()
    {
        iterator x(this);
        x.end();
        return x;
    }




    //v_list.begin()
    /////////////////////////////////////////////////////////////////////////////////////////
    //        // iterators
    //        list::iterator  begin() _GLIBCXX_NOEXCEPT      { return v_list.begin(); }

    //        const_iterator  begin() const _GLIBCXX_NOEXCEPT     { return v_list.begin();}


    //        iterator        end() _GLIBCXX_NOEXCEPT        { return iterator(&this->_M_impl._M_node); }


    //        const_iterator        end() const _GLIBCXX_NOEXCEPT        { return const_iterator(&this->_M_impl._M_node); }


    //        reverse_iterator        rbegin() _GLIBCXX_NOEXCEPT        { return reverse_iterator(end()); }


    //        const_reverse_iterator        rbegin() const _GLIBCXX_NOEXCEPT        { return const_reverse_iterator(end()); }

    //        reverse_iterator        rend() _GLIBCXX_NOEXCEPT        { return reverse_iterator(begin()); }


    //        const_reverse_iterator        rend() const _GLIBCXX_NOEXCEPT        { return const_reverse_iterator(begin()); }

    //  #if __cplusplus >= 201103L

    //        const_iterator        cbegin() const noexcept        { return const_iterator(this->_M_impl._M_node._M_next); }

    //        const_iterator        cend() const noexcept        { return const_iterator(&this->_M_impl._M_node); }

    //        const_reverse_iterator        crbegin() const noexcept        { return const_reverse_iterator(end()); }

    //         const_reverse_iterator        crend() const noexcept        { return const_reverse_iterator(begin()); }
    //  #endif

    //        // [23.2.2.2] capacity
    //        bool        empty() const _GLIBCXX_NOEXCEPT        { return this->_M_impl._M_node._M_next == &this->_M_impl._M_node; }

    //        size_type        size() const _GLIBCXX_NOEXCEPT        { return this->_M_node_count(); }

    //        size_type        max_size() const _GLIBCXX_NOEXCEPT        { return _Node_alloc_traits::max_size(_M_get_Node_allocator()); }

    /////////////////////////////////////////////////////////////////////////////////////////
    //        void test()
    //        {
    //            Value root;
    //            for(auto &x:root)
    //            {

    //            }
    //        }

    //        Value *owner=NULL;

    Value *prt()
    {
        return this;
    }

    ////////////////////
    ~Value()
    {
        clearAll();
    }
    Value():t(NUL)
    {

    }
    void clearAll()
    {
        v_list.clear();
        v_object.clear();
    }


    ////////////////////


    Value(const int v)             { t = NUMBER;  v_number = v;}
    Value(const long v)            { t = NUMBER;  v_number = v;}
    Value(const char v)            { t = NUMBER;  v_number = v;}
    Value(const short v)           { t = NUMBER;  v_number = v;}
    Value(const long long v)       { t = NUMBER;  v_number = v;}
    Value(const unsigned int v)    { t = NUMBER;  v_number = v;}
    Value(const unsigned long v)   { t = NUMBER;  v_number = v;}
    Value(const unsigned char v)   { t = NUMBER;  v_number = v;}
    Value(const unsigned short v)  { t = NUMBER;  v_number = v;}
    Value(const bool v)            { t = BOOL;    v_number = v;}
    Value(const string &v)         { t = STRING;  v_string = v;}
    Value(const double v)          { t = DOUBLE;  v_double = v;}
    Value(const float v)           { t = DOUBLE;  v_double = v;}


    Value(const char *v)
    {
        if(v==NULL)
        {
            t=NUL;
        }else
        {
            t = STRING;
            v_string = string(v);
        }
    }
    Value(const Type v)
    {
        if(v==NUL)  t=NUL;
        else if(v==ARRAY)  t = ARRAY;
        else if(v==OBJECT)  t = OBJECT;
        else
        {
            throw SmyExp::ValueUnknowErr("constructer use Type can only use NUL");
        }
    }

    ////////////////////
    Value(const list<Value> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<int> &v)       { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<long> &v)      { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<char> &v)      { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<short> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<long long> &v)         { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<unsigned int> &v)      { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<unsigned long> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<unsigned char> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<unsigned short> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<bool> &v)      { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<string> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<char*> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<double> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const list<float> &v)     { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}

    ////////////////////


    Value(const vector<Value> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}

    Value(const vector<int> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<long> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<char> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<short> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<long long> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<unsigned int> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<unsigned long> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<unsigned char> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<unsigned short> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<bool> &v)    { t = ARRAY;  for(auto x:v) v_list.push_back(x);}
    Value(const vector<string> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<char*> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<double> &v)   { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const vector<float> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}

    ////////////////////


    Value(const initializer_list<Value> &v)    { t = ARRAY;  for(auto &x:v) v_list.push_back(x);}
    Value(const initializer_list<pair<string, Value>> &v)          { t = OBJECT;  for(const pair<string, Value> &x:v) v_object[x.first] = x.second;}


    ////////////////////

    Value(const Value &v)
    {
        t = v.t;

        if(v.t==ARRAY)
        {
            for(auto &x:v.v_list) v_list.push_back(x);

        }else if(v.t==OBJECT)
        {
            for(auto &xx:v.v_object)
            {
                string key = xx.first;
                v_object[key] = xx.second;
                v_object[key].key = key;
            }

        }else if(v.t==NUMBER)
        {
            v_number = v.v_number;
        }else if(v.t==DOUBLE)
        {
            v_double = v.v_double;
        }else if(v.t==BOOL)
        {
            v_number = v.v_number;
        }else if(v.t==STRING)
        {
            v_string = v.v_string;
        }else //NUL
        {

        }

    }

    ////////////////////

    Value& operator [] (const string &key)
    {
        if( (t!=OBJECT) && (t!=NUL) )
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Object or NUL.\n",typeStr[t].data());
        }
        if(t==NUL)
        {
            t = OBJECT;
        }
        Value &v = v_object[key];
        v.key = key;
        return v;
    }
    Value& operator [] (const char *key)
    {
        string s(key);
        return (*this)[s];
    }

    Value& operator [] (int pos)//不支持自动增长。
    {
        if(t!=ARRAY)
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Array.\n",typeStr[t].data());
        }

        int count = v_list.size();
        if((pos<0) || (pos>=count))
        {
            throw SmyExp::ArrayPosErr("array pos err. array size=%d, want pos=%d.\n",count, pos);
        }

        int k=pos;
        for(auto &x:v_list)
        {
            k--;
            if(k<0) return x;
        }

        //can not run to this line;
        throw SmyExp::ArrayPosErr("array pos err. array size=%d, want pos=%d.\n",count, pos);
        return *this;
    }
    Value& at(int pos)
    {
        return (*this)[pos];
    }

    void append(const Value &v)
    {
        if( (t!=ARRAY) && (t!=NUL) )
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Array or NUL.\n",typeStr[t].data());
        }
        if(t==NUL)
        {
            t=ARRAY;
        }
        v_list.push_back(v);
    }
    void insert(int pos, const Value &v)
    {
        if( (t!=ARRAY) && (t!=NUL) )
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Array or NUL.\n",typeStr[t].data());
        }
        if(t==NUL)
        {
            t=ARRAY;
        }
        int count = v_list.size();
        if(pos<0)
        {
            throw SmyExp::ArrayPosErr("array pos err. array size=%d, want pos=%d.\n",count, pos);
        }
        if(pos>=count)
        {
            v_list.push_back(v);
        }else
        {
            list<Value>::iterator it = v_list.begin();
            advance(it,pos);
            v_list.insert(it,v);
        }

    }
    void erase(int pos)
    {
        if(t!=ARRAY)
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Array.\n",typeStr[t].data());
        }
        int count = v_list.size();
        if((pos<0) || (pos>=count))
        {
            throw SmyExp::ArrayPosErr("array pos err. array size=%d, want pos=%d.\n",count, pos);
        }

        list<Value>::iterator it = v_list.begin();
        advance(it,pos);
        v_list.erase(it);

    }
    void erase(const string &key)
    {
        if(t!=OBJECT)
        {
            throw SmyExp::OperatorErrNotObject("type is [%s], not Object.\n",typeStr[t].data());
        }
        v_object.erase(key);
    }


    ////////////////////

    Value &operator = (const int  v)            { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const long v)            { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const char v)            { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const short v)           { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const long long v)       { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const unsigned int v)    { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const unsigned long v)   { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const unsigned char v)   { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const unsigned short v)  { clearAll();  t = NUMBER;  v_number = v; return *this;}
    Value &operator = (const bool v)            { clearAll();  t = BOOL;    v_number = v; return *this;}
    Value &operator = (const string &v)         { clearAll();  t = STRING;  v_string = v; return *this;}
    Value &operator = (const char *v)
    {
        clearAll();
        if(v==NULL)
        {
            t=NUL;
        }else
        {
            t = STRING;
            v_string = string(v);
        }
        return *this;
    }

    Value &operator = (const Type v)
    {
        clearAll();
        if(v==NUL)  t=NUL;
        else if(v==ARRAY)  t = ARRAY;
        else if(v==OBJECT)  t = OBJECT;
        else
        {
            throw SmyExp::ValueUnknowErr("operator = func,  use Type can only use NUL");
        }
        return *this;
    }

    Value &operator = (const double v)          { clearAll();  t = DOUBLE;  v_double = v; return *this;}
    Value &operator = (const float v)           { clearAll();  t = DOUBLE;  v_double = v; return *this;}

    ////////////////////

    Value &operator = (const list<Value> &v)     { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }

    Value &operator = (const list<int> &v)      { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<long> &v)     { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<char> &v)     { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<short> &v)            { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<long long> &v)        { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<unsigned int> &v)     { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<unsigned long> &v)    { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<unsigned char> &v)    { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<unsigned short> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<bool> &v)     { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<string> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<char *> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<double> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const list<float> &v)    { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    ////////////////////
    Value &operator = (const vector<Value> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<int> &v)    { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<long> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<char> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<short> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<long long> &v)      { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<unsigned int> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<unsigned long> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<unsigned char> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<unsigned short> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<bool> &v)   { clearAll();t = ARRAY;for(auto x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<string> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<char *> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<double> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const vector<float> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    ////////////////////

    Value &operator = (const initializer_list<Value> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<int> &v)    { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<long> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<char> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<short> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<long long> &v)      { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<unsigned int> &v)   { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<unsigned long> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<unsigned char> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<unsigned short> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<bool> &v)   { clearAll();t = ARRAY;for(auto x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<string> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<char *> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<double> &v) { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }
    Value &operator = (const initializer_list<float> &v)  { clearAll();t = ARRAY;for(auto &x:v) v_list.push_back(x);  return *this;  }

    ////////////////////
    Value &operator = (const initializer_list<pair<string, Value>> &v)  { clearAll();t = ARRAY;for(const pair<string, Value> &x:v) v_object[x.first] = x.second;  return *this;  }

    ////////////////////

    operator int() { return this->toInt(); }
    operator double() { return this->toDouble(); }
    operator float() { return this->toDouble(); }
    operator unsigned int() {return  (unsigned int)this->toInt(); }
    operator std::string(){return this->toString();}

    operator list<string>(){return this->toStringList();}
    operator vector<string>(){return this->toStringVector();}
    operator vector<int>(){return this->toIntVector();}
    operator vector<unsigned int>(){return this->toUIntVector();}
    operator vector<float>(){return this->toFloatVector();}
    operator vector<double>(){return this->toDoubleVector();}




    Value &operator = (const Value v)
    {
        clearAll();

        t = v.t;

        if(v.t==ARRAY)
        {
            for(auto &x:v.v_list) v_list.push_back(x);

        }else if(v.t==OBJECT)
        {
            for(auto &xx:v.v_object)
            {
                string key = xx.first;
                v_object[key] = xx.second;
                v_object[key].key = key;

            }

        }else if(v.t==NUMBER)
        {
            v_number = v.v_number;
        }else if(v.t==DOUBLE)
        {
            v_double = v.v_double;
        }else if(v.t==BOOL)
        {
            v_number = v.v_number;
        }else if(v.t==STRING)
        {
            v_string = v.v_string;
        }else //NUL
        {

        }

        return *this;
    }

    /////////////////////////

    string  toString()
    {
        switch(t)
        {
        case STRING:
        {
            return v_string;
        }break;
        case BOOL:
        {
            if(v_number) return string("true");
            else return string("false");
        }break;
        case DOUBLE:
        {
            string value = string_format("%.15lf",v_double);
            truncatTailingZeroes(value);
            return value;
        }break;
        case NUMBER:
        {
            return string_format("%lld", v_number);
        }break;
        case NUL:
        {
            return string("null");
        }break;
        default:
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        }break;
        }
    }
    int toHexInt()
    {
        switch(t)
        {
        case STRING:
        {
            string s = v_string;
            remove_if(s.begin(),s.end(),isSpace);
            transform(s.begin(), s.end(), s.begin(), ::tolower);
            int x;
            if(sscanf(s.data(), "0x%x", &x)==1)
            {
                return x;
            }else if(sscanf(s.data(), "%x", &x)==1)
            {
                return x;
            }else
            {
                return 0;
            }

        }break;
        default:
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }break;
        }
    }

    int toInt()
    {
        switch(t)
        {
        case STRING:
        {
            string s = v_string;
            remove_if(s.begin(),s.end(),isSpace);
            transform(s.begin(), s.end(), s.begin(), ::tolower);
            if(s=="true") return 1;
            else if(s=="false") return 0;
            else
            {
                double x= stod(s.data());
                return x;
            }
        }break;
        case BOOL:
        case NUMBER:
        {
            return v_number;
        }break;
        case DOUBLE:
        {
            return v_double;
        }break;
        case NUL:
        {
            return 0;
        }break;
        default:
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }break;
        }

    }
    double  toDouble()
    {
        switch(t)
        {
        case STRING:
        {
            string s = v_string;
            remove_if(s.begin(),s.end(),isSpace);
            transform(s.begin(), s.end(), s.begin(), ::tolower);
            if(s=="true") return 1;
            else if(s=="false") return 0;
            else
            {
                double x= stod(s.data());
                return x;
            }
        }break;
        case BOOL:
        case NUMBER:
        {
            return v_number;
        }break;
        case DOUBLE:
        {
            return v_double;
        }break;
        case NUL:
        {
            return 0;
        }break;
        default:
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }break;
        }
    }



    bool    toBool()
    {
        switch(t)
        {
        case STRING:
        {
            string s = v_string;
            remove_if(s.begin(),s.end(),isSpace);
            transform(s.begin(), s.end(), s.begin(), ::tolower);
            if(s=="true") return true;
            else if(s=="false") return false;
            else
            {
                double x= stod(s.data());
                if(x>zero_double) return true;
                if(x<-zero_double) return true;
                return false;
            }
        }break;
        case BOOL:
        case NUMBER:
        {
            return v_number;
        }break;
        case DOUBLE:
        {
            if(v_double>zero_double) return true;
            if(v_double<-zero_double) return true;
            return false;
        }break;
        case NUL:
        {
            return 0;
        }break;
        default:
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }break;
        }
    }

    long long &asLongLong()
    {
        if(t==NUMBER)
        {
            return v_number;
        }else
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }
    }
    string  &asString()
    {
        if(t==STRING)
        {
            return v_string;
        }else
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }
    }
    double  &asDouble()
    {
        if(t==DOUBLE)
        {
            return v_double;
        }else
        {
            throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
        }
    }

    /////////////////////////
    vector<string> toStringVector()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        vector<string> ls;
        for(Value &x:v_list)
        {
            if(x.t!=STRING) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_string);

        }
        return ls;
    }
    list<string> toStringList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        list<string> ls;
        for(Value &x:v_list)
        {
            if(x.t!=STRING) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_string);

        }
        return ls;
    }

    vector<int> toIntVector()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        vector<int> ls;
        for(Value &x:v_list)
        {
            if(x.t!=NUMBER) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_number);

        }
        return ls;
    }
    vector<unsigned int> toUIntVector()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        vector<unsigned int> ls;
        for(Value &x:v_list)
        {
            if(x.t!=NUMBER) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back((unsigned int)x.v_number);

        }
        return ls;
    }
    vector<double> toDoubleVector()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        vector<double> ls;
        for(Value &x:v_list)
        {
            if(x.t!=DOUBLE) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_double);

        }
        return ls;
    }
    vector<float> toFloatVector()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        vector<float> ls;
        for(Value &x:v_list)
        {
            if(x.t!=DOUBLE) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_double);

        }
        return ls;
    }








    list<Value> &asList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        return v_list;
    }

    map<string, Value> &asMap()
    {
        if(t!=OBJECT) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        return v_object;
    }


    list<int> toIntList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        list<int> ls;
        for(Value &x:v_list)
        {
            if(x.t!=NUMBER) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_number);

        }
        return ls;
    }
    list<long long> toLongLongList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        list<long long> ls;
        for(Value &x:v_list)
        {
            if(x.t!=NUMBER) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_number);

        }
        return ls;
    }
    list<double> toDoubleList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        list<double> ls;
        for(Value &x:v_list)
        {
            if(x.t!=DOUBLE) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_double);

        }
        return ls;
    }

    list<float> toFloatList()
    {
        if(t!=ARRAY) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());

        list<float> ls;
        for(Value &x:v_list)
        {
            if(x.t!=DOUBLE) throw SmyExp::OperatorErrType("type is [%s], can not convert.\n",typeStr[t].data());
            ls.push_back(x.v_double);

        }
        return ls;
    }

    /////////////////////////

    bool isObject()     { return t==OBJECT; }
    bool isList()       { return t==ARRAY;}
    bool isString()     { return t==STRING; }
    bool isBool()       { return t==BOOL; }
    bool isFloat()      { return t==DOUBLE;  }
    bool isNumber()     { return t==NUMBER; }
    bool isNull()       { return t==NUL;  }
    bool isCommonChildrens()
    {
        if(this->isList())
        {
            for(Value &x:v_list)
            {
                if(x.isObject()) return false;
                if(x.isList()) return false;
            }
            return true;
        }else if(this->isObject())
        {
            for(auto &x:v_object)
            {
                Value &value = x.second;

                if(value.isObject()) return false;
                if(value.isList()) return false;
            }
            return true;
        }else
        {
            return true;
        }

    }

    /////////////////////////

    void clear()        { *this = Value(); }

    Type type()
    {
        return t;
    }
    /////////////////////////

    void truncatTailingZeroes(std::string &tmps)
    {
        //删除尾部多余的0，如果尾部以点结束，也删除小数点
        if(tmps.find(".")>0)
        {
            size_t fp = tmps.rfind(".");
            size_t f = tmps.rfind("0");
            while (f > fp) {
                if(f == string::npos) break;
                tmps = tmps.erase(f);
                f = tmps.rfind("0");
            }
            fp = tmps.rfind(".");
            if(fp == tmps.size() - 1)
            {
                tmps = tmps.erase(fp);
            }
        }
    }

    string serialize(string endStr="\n", string tabStr="  ", int combine=0, int tab=0)
    {
        string str;

        switch(t)  {
        case OBJECT:
        {
            str="{";
            tab++;

            if(!combine || !this->isCommonChildrens())
            {
                str+=endStr;
                //for(int i=0;i<tab;i++) str+=tabStr;
            }

            int count = v_object.size();
            int combine_line_count=count;
            int combine_line_cur=0;
            for(auto &x:v_object)
            {
                Value &value = x.second;

                //tab
                if(!combine || !this->isCommonChildrens())
                {
                    //str+=endStr;
                    for(int i=0;i<tab;i++) str+=tabStr;
                }

                //key
                str += "\"";
                string key = x.first;
                if(key.length()<1)
                {
                    throw SmyExp::SeralizeErr("");
                }
                str+=key;

                //:
                str+="\":";

                //value
                {

                    string value_str;
                    if(value.t==OBJECT)
                    {
                        value_str = value.serialize(endStr,tabStr, combine,tab);
                    }else
                    {
                        value_str = value.serialize(endStr,tabStr, combine,tab+1);
                    }
                    if(value_str.length()<1)
                    {
                        throw SmyExp::SeralizeErr("");
                    }
                    str+=value_str;
                    if(--count>0) str+=",";

                    if(!combine || !this->isCommonChildrens())
                    {
                        str+=endStr;
                        //for(int i=0;i<tab;i++) str+=tabStr;
                    }else
                    {
                        ++combine_line_cur;
                        if((combine_line_cur >= combine) && (combine_line_cur<combine_line_count) )
                        {
                            combine_line_cur=0;
                            str+=endStr;
                            for(int i=0;i<tab;i++) str+=tabStr;
                        }else
                        {
                            str+=" \t";
                        }
                    }

                    //if(combine) str +=tabStr;
                    //else str+=endStr;

                }
            }

            tab--;

            if( !combine  ||  !this->isCommonChildrens())
            {
                for(int i=0;i<tab;i++) str+=tabStr;
            }
            str+="}";

            return str;
        }break;
        case ARRAY:
        {
            str="[";

            tab++;

            int count = v_list.size();
            Type tp = NUL;
            if(count>0)
            {
                Value &vt = v_list.front();
                tp = vt.t;
            }

            if( !combine  ||  !this->isCommonChildrens())
            {
                str+=endStr;
            }

            int combine_line_cur=0;
            int combine_line_count=count;
            for(auto &value:v_list)
            {
                //tab
                //内部为复杂类型， 进行回车分块。同时tp！=NULL时，列表个数>0
                if( !combine  ||  !this->isCommonChildrens())
                    for(int i=0;i<tab;i++) str+=tabStr;
                //else if(tabStr.length()>0) str+="\t";//基本类型


                //value
                {
                    string value_str = value.serialize(endStr, tabStr, combine, tab);
                    if(value_str.length()<1)
                    {
                        throw SmyExp::SeralizeErr("");
                    }
                    str+=value_str;
                    if(--count>0) str+=",";

                    //内部为复杂类型， 进行回车分块。同时tp！=NULL时，列表个数>0
                    if( !combine  ||  !this->isCommonChildrens())
                    {
                       // if(tab>combine) str +=tabStr;
                       // else
                        str+=endStr;
                    }else
                    {
                        ++combine_line_cur;
                        if((combine_line_cur >= combine) && (combine_line_cur<combine_line_count) )
                        {
                            combine_line_cur=0;
                            str+=endStr;
                            for(int i=0;i<tab;i++) str+=tabStr;
                        }else
                        {
                            str+=" \t";
                        }
                    }

                }
            }

            tab--;
            //内部为复杂类型， 进行回车分块。同时tp！=NULL时，列表个数>0
            if( !combine  ||  !this->isCommonChildrens())
                for(int i=0;i<tab;i++) str+=tabStr;
            str+="]";
            return str;
        }break;
        case STRING:
        {
            string value = string_format("\"%s\"",v_string.data());
            return value;
        }break;
        case BOOL:
        {
            if(v_number) return string("true");
            else return string("false");
        }break;
        case DOUBLE:
        {
            string value = string_format("%.15lf",v_double);
            truncatTailingZeroes(value);
            return value;
        }break;
        case NUMBER:
        {
            return string_format("%lld", v_number);
        }break;
        case NUL:
        {
            return string("null");
        }break;
        default:
        {
            throw SmyExp::SeralizeErr("unknow type:%d\n", t);

        }break;
        }

        return str;
    }

};





class Element
{
public:
    Element(){

    }

    int start_pos=0;
    int end_pos=0;
    string str="";
    int lineNo;
    int linePos;
    Type type;

    string toString()
    {
        string s;
        if(type != STRING)
        {
            throw SmyExp::PasserEleErr("Element passer err,toString() err, type=%s\n", typeStr[type].data());
        }
        if(str.length()<2)
        {
            throw SmyExp::PasserEleErr("Element passer err,toString() err, length<2 err, lenght=%d\n",s.length());
        }
        s = str.substr(1, str.length()-2);
        return s;
    }
    string toKey()
    {
        string s;
        if(type != STRING)
        {
            throw SmyExp::PasserEleErr("Element passer err,toKey() err, type=%s\n", typeStr[type].data());
        }
        if(str.length()<2)
        {
            throw SmyExp::PasserEleErr("Element passer err,toKey() err, length<2 err, lenght=%d\n",s.length());
        }
        s = str.substr(1, str.length()-2);
        return s;
    }
    long long toNumber()
    {
        long long n;
        sscanf(str.data(), "%lld",&n);
        return n;
    }
    double toDouble()
    {
        double n;
        sscanf(str.data(), "%lf",&n);
        return n;
    }
    bool toBool()
    {
        if(str=="true") return true;
        else if(str=="false") return false;
        else throw SmyExp::PasserEleErr("Element passer err,toBool() err, str unknow, str=%s\n",str.data());
    }

    bool operator ==(Type v)
    {
        return this->type==v;
    }

};


//return        是否完整
//dealStrLen    已经处理的字符个数。

bool  passerString2Elements(const string &str,list<Element> &elements, int *dealStrLen)
{
    elements.clear();

    int len = str.length();

    //
    int lineNo=0;
    int linePos=0;

    int start_pos=0;

    //char start=0;  // q, b,n,s,d 标记第一个元素类型。
    int eleCnt=0;//元素个数
    int blCnt = 0;//计算大括号
    int mlCnt = 0;//计算中括号

    for(start_pos=0;start_pos<len;start_pos++)
    {
        char c = str.at(start_pos);

        if(c=='\n')
        {
            lineNo++;
            linePos=start_pos;
            continue;
        }

        if(isspace(c))
        {
            continue;
        }

        if(eleCnt==1)//第一个元素
        {
            Element &ele = elements.front();
            switch(ele.type)
            {
            case NUL:
            case NUMBER:
            case DOUBLE:
            case BOOL:
            case STRING:{
                // ok only one elements.
                *dealStrLen = start_pos;
                return true;
            }break;
                //case ARRAY:{}break;
                // case OBJECT:{}break;
            case BraceL:
            case BracketL:{
                // ok find start with object or array.

            }break;
            case BraceR:
            case BracketR:
            case Colon:
            case Comma:{
                // err. start with an err element.
                *dealStrLen = start_pos;
                return false;
            }break;
            default:{
                // err. start with an unknow element.
                *dealStrLen = start_pos;
                return false;
            }break;
            }

        }else if(eleCnt>1)
        {
            if( (blCnt==0) && (mlCnt==0))
            {
                // ok find the end.
                *dealStrLen = start_pos;
                return true;
            }
        }

        eleCnt++;
        if( (c=='{') || (c=='}') || (c=='[') || (c==']') || (c==',') || (c==':'))
        {
            // if(!start) start='q';
            Element ele;
            ele.lineNo = lineNo;
            ele.linePos = start_pos-linePos;
            ele.start_pos=start_pos;
            ele.str = string(1,c);
            ele.end_pos = start_pos+(1-1);

            // ele.type = Sep;

            if     ((c=='{') )     { ele.type = BraceL; blCnt++;}
            else if((c=='}') )     { ele.type = BraceR; blCnt--;}
            else if((c=='[') )     { ele.type = BracketL; mlCnt++;}
            else if((c==']') )     { ele.type = BracketR; mlCnt--;}
            else if((c==':') )      ele.type = Colon;
            else /* ((c==',') )*/   ele.type = Comma;

            elements.push_back(ele);
            start_pos+=1-1;
            continue;
        }
        if( (c=='t') || (c=='f') )
        {
            // if(!start) start='b';
            if(str.substr(start_pos,4)=="true")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "true";
                ele.end_pos = start_pos+(4-1);
                ele.type = BOOL;

                elements.push_back(ele);
                start_pos+=4-1;

                continue;
            }else if(str.substr(start_pos,5)=="false")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "false";
                ele.end_pos = start_pos+(5-1);
                ele.type = BOOL;

                elements.push_back(ele);
                start_pos+=5-1;

                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }
        if(c=='n')
        {
            // if(!start) start='n';
            if(str.substr(start_pos,4)=="null")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "null";
                ele.end_pos = start_pos+(4-1);
                ele.type = NUL;

                elements.push_back(ele);
                start_pos+=4-1;

                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }
        if (c=='\"')
        {
            // if(!start) start='s';
            if(start_pos+1>=len)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            int end=start_pos+1;
            for(end=start_pos+1;end<len;end++)
            {
                char cc = str.at(end);
                if( isspace(cc) && (cc!=' '))
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }

                if( (str.at(end)=='\"') && (str.at(end-1)!='\\')) break;
            }
            if(end<len)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.end_pos = start_pos+(end-start_pos+1-1);

                ele.str = str.substr(start_pos,end-start_pos+1);
                ele.type = STRING;

                elements.push_back(ele);
                start_pos += end-start_pos;
                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }


        }
        if( isdigit(c) || (c=='-'))
        {
            // if(!start) start='d';
            bool hasPoint=false;
            bool subFlag=false;

            int off=0;
            if(c=='-')
            {
                if(subFlag)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }
                subFlag=true;
                off++;
                if(start_pos+1>=len)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }
                //start_pos++;
            }

            if(start_pos+off>=len)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            c = str.at(start_pos+off);
            if(!isdigit(c))
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            off++;
            for(;start_pos+off<len;off++)
            {
                c = str.at(start_pos+off);
                if(c=='.')
                {
                    if(hasPoint)
                    {
                        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                        //err
                    }else
                    {
                        hasPoint=true;
                        continue;
                    }
                }

                if(!isdigit(c))
                {
                    break; // digit end.
                }

                //is digit ok, find next
            }
            if(start_pos+off<len)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.end_pos = start_pos+(off+1 -1-1);
                ele.str = str.substr(start_pos,off+1 -1);
                if(hasPoint) ele.type = DOUBLE;
                else         ele.type = NUMBER;

                elements.push_back(ele);
                start_pos += off -1;
                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }


        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
        //err

    }//for

    //err end with no end.
    *dealStrLen = start_pos;

    if(eleCnt>0)
    {
        if( (blCnt==0) && (mlCnt==0)) return true;
        else return false;//未发现结尾
    }else
    {
        return false;//未发现开头
    }

}


//-1: err
//1: end
//0: continue
static int _elesCheck(list<Element> &els, int bl_cnt, int ml_cnt)
{

    if(els.size()==1)
    {
        Element &e = els.front();

        if( (e.type!=BraceL) && (e.type != BracketL) )
        {
            switch(e.type)
            {
            case NUL:
            case NUMBER:
            case DOUBLE:
            case BOOL:
            case STRING:{
                return 1;
            }break;
            default:{
                return -1;
            }
            }//switch
        }else
        {
            return 0;
        }
    }else
    {
        if(ml_cnt<0)
        {
            return -1;
        }else if(bl_cnt<0)
        {
            return -1;
        }else if( (bl_cnt==0) && (ml_cnt==0))
        {
            return 1;
        }else
        {
            return 0;
        }
    }
}
template <class T>
static void _read_blank(T   &in,int &dealLen)
{
    char c;
    while(smy_read(in,&c,1)==1)
    {
        dealLen++;
        if(!isSpace(c))
        {
            smy_unread(in,&c,1);
            dealLen--;
            return;
        }
    }
    return;

}

template <class T>
static bool passerStream2Elements(T &in, list<Element> &elements, int &dealLen)
//bool passerStream2Elements(stringstream   &in, list<Element> &elements, int &dealLen)
{
    elements.clear();

    //
    int lineNo=0;
    int linePos=0;

    int bl_cnt=0;
    int ml_cnt=0;

    dealLen=0;

    while(1)
    {
        int n=0;

        char buf[StringMaxLen];

        if(smy_read(in, &buf[0],1)!=1)
        {
            break;  //get end.
        }
        n++;
        linePos++;
        dealLen++;


        if(buf[0]=='\n')
        {
            lineNo++;
            linePos = 0;
            continue;
        }

        if(isspace(buf[0]))
        {
            continue;
        }

        if( (buf[0]=='{') || (buf[0]=='}') || (buf[0]=='[') || (buf[0]==']') || (buf[0]==',') || (buf[0]==':'))
        {
            Element ele;
            ele.lineNo = lineNo;
            ele.linePos = linePos;
            ele.str = string(1,buf[0]);

            if     ((buf[0]=='{') )     {bl_cnt++; ele.type = BraceL;}
            else if((buf[0]=='}') )     {bl_cnt--; ele.type = BraceR;}
            else if((buf[0]=='[') )     {ml_cnt++; ele.type = BracketL;}
            else if((buf[0]==']') )     {ml_cnt--; ele.type = BracketR;}
            else if((buf[0]==':') )     {ele.type = Colon;}
            else /* ((buf[0]==',') )*/  {ele.type = Comma;}

            elements.push_back(ele);
            int chk = _elesCheck(elements, bl_cnt, ml_cnt);
            if(chk==1)
            {
                _read_blank(in,dealLen);
                return true;
            }
            if(chk==-1)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            }

            continue;
        }
        if( (buf[0]=='t') || (buf[0]=='f') )
        {
            if(smy_read(in, &buf[n],3)!=3)// << tre, als
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            }
            n+=3;
            linePos+=3;
            dealLen+=3;

            if(strncmp(buf,"true",4)==0)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = linePos;
                ele.str = "true";
                ele.type = BOOL;

                elements.push_back(ele);
                int chk = _elesCheck(elements, bl_cnt, ml_cnt);
                if(chk==1)
                {
                    _read_blank(in,dealLen);
                    return true;
                }
                if(chk==-1)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                }
                continue;
            }

            if(smy_read(in, &buf[4],1)!=1)// << tre, als
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            }
            n+=1;
            linePos+=1;
            dealLen+=1;

            if(strncmp(buf,"false",5)==0)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = linePos;
                ele.str = "false";
                ele.type = BOOL;

                elements.push_back(ele);
                int chk = _elesCheck(elements, bl_cnt, ml_cnt);
                if(chk==1)
                {
                    _read_blank(in,dealLen);
                    return true;
                }
                if(chk==-1)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                }
                continue;
            }

            throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);

        }
        if(buf[0]=='n')
        {
            if(smy_read(in, &buf[1],3)!=3)// << tre, als
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            }
            n+=3;
            linePos += 3;
            dealLen+=3;

            if(strncmp(buf,"null",4)==0)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = linePos;
                ele.str = "null";
                ele.type = NUL;

                elements.push_back(ele);
                int chk = _elesCheck(elements, bl_cnt, ml_cnt);
                if(chk==1)
                {
                    _read_blank(in,dealLen);
                    return true;
                }
                if(chk==-1)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                }
                continue;
            }

            throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            //err
        }

        if (buf[0]=='\"')
        {
            //n=1;
            while( (smy_read(in, &buf[n],1)==1) &&  (n<StringMaxLen) )
            {
                n++;
                linePos++;
                dealLen++;

                if( isspace(buf[n-1]) && (buf[n-1]!=' '))
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                    //err
                }

                if( (buf[n-1]=='\"') && (n>=2) && (buf[n-2]!='\\'))
                {
                    Element ele;
                    ele.lineNo = lineNo;
                    ele.linePos = linePos;

                    ele.str = char2string(buf,n);
                    ele.type = STRING;

                    elements.push_back(ele);
                    int chk = _elesCheck(elements, bl_cnt, ml_cnt);
                    if(chk==1)
                    {
                        _read_blank(in,dealLen);
                        return true;
                    }
                    if(chk==-1)
                    {
                        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                    }

                    goto get_string_end_ok;
                }
            }
            if(n>=StringMaxLen)
            {
                throw SmyExp::ArrayPosErr("line:%d, element char pos:%d", lineNo,linePos);
                //err
            }

            throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            //err

get_string_end_ok:
            continue;
        }

        if( isdigit(buf[0]) || (buf[0]=='-'))
        {
            bool hasPoint=false;

            if(buf[n-1]=='-')
            {
                if(smy_read(in, &buf[n],1)!=1)// << tre, als
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                }
                n+=1;
                linePos += 1;
                dealLen += 1;
            }

            if(!isdigit(buf[n-1]))
            {
                smy_unread(in, &buf[0],n); // un read
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                //err
            }

            while( (smy_read(in, &buf[n],1)==1) &&  (n<StringMaxLen) )
            {
                n++;
                linePos++;
                dealLen++;

                if(buf[n-1]=='.')
                {
                    if(hasPoint)
                    {
                        smy_unread(in, &buf[0],n); // un read
                        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
                        //err
                    }else
                    {
                        hasPoint=true;
                        continue;
                    }
                }

                if(!isdigit(buf[n-1]))
                {
                    smy_unread(in, &buf[n-1],1); // un read
                    n--;
                    linePos--;
                    dealLen--;
                    goto get_digit_ok; // digit end.
                }

                //is digit ok, find next
            }
            if(n>=StringMaxLen)
            {
                throw SmyExp::ArrayPosErr("line:%d, element char pos:%d", lineNo,linePos);
                //err
            }

            throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            //err
get_digit_ok:
            Element ele;
            ele.lineNo = lineNo;
            ele.linePos = linePos;
            ele.str = char2string(buf,n);
            if(hasPoint) ele.type = DOUBLE;
            else         ele.type = NUMBER;

            elements.push_back(ele);
            int chk = _elesCheck(elements, bl_cnt, ml_cnt);
            if(chk==1) return true;
            if(chk==-1)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
            }

            continue;

        }

        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
        //err

    }//while

    // simple check
    if(elements.size()==0) return true;

    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,linePos);
}


bool passerString2Elements(const string &str,list<Element> &elements)
{
    elements.clear();

    int len = str.length();

    //
    int lineNo=0;
    int linePos=0;

    int start_pos=0;
    for(start_pos=0;start_pos<len;start_pos++)
    {
        char c = str.at(start_pos);

        if(c=='\n')
        {
            lineNo++;
            linePos=start_pos;
            continue;
        }

        if(isspace(c))
        {
            continue;
        }

        if( (c=='{') || (c=='}') || (c=='[') || (c==']') || (c==',') || (c==':'))
        {
            Element ele;
            ele.lineNo = lineNo;
            ele.linePos = start_pos-linePos;
            ele.start_pos=start_pos;
            ele.str = string(1,c);
            ele.end_pos = start_pos+(1-1);

            // ele.type = Sep;

            if     ((c=='{') )      ele.type = BraceL;
            else if((c=='}') )      ele.type = BraceR;
            else if((c=='[') )      ele.type = BracketL;
            else if((c==']') )      ele.type = BracketR;
            else if((c==':') )      ele.type = Colon;
            else /* ((c==',') )*/   ele.type = Comma;

            elements.push_back(ele);
            start_pos+=1-1;
            continue;
        }
        if( (c=='t') || (c=='f') )
        {
            if(str.substr(start_pos,4)=="true")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "true";
                ele.end_pos = start_pos+(4-1);
                ele.type = BOOL;

                elements.push_back(ele);
                start_pos+=4-1;

                continue;
            }else if(str.substr(start_pos,5)=="false")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "false";
                ele.end_pos = start_pos+(5-1);
                ele.type = BOOL;

                elements.push_back(ele);
                start_pos+=5-1;

                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }
        if(c=='n')
        {
            if(str.substr(start_pos,4)=="null")
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.str = "null";
                ele.end_pos = start_pos+(4-1);
                ele.type = NUL;

                elements.push_back(ele);
                start_pos+=4-1;

                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }

        if (c=='\"')
        {
            if(start_pos+1>=len)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            int end=start_pos+1;
            for(end=start_pos+1;end<len;end++)
            {
                char cc = str.at(end);
                if( isspace(cc) && (cc!=' '))
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }

                if( (str.at(end)=='\"') && (str.at(end-1)!='\\')) break;
            }
            if(end<len)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.end_pos = start_pos+(end-start_pos+1-1);

                ele.str = str.substr(start_pos,end-start_pos+1);
                ele.type = STRING;

                elements.push_back(ele);
                start_pos += end-start_pos;
                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }


        }

        if( isdigit(c) || (c=='-'))
        {
            bool hasPoint=false;
            bool subFlag=false;

            int off=0;
            if(c=='-')
            {
                if(subFlag)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }
                subFlag=true;
                off++;
                if(start_pos+1>=len)
                {
                    throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                    //err
                }
                //start_pos++;
            }

            if(start_pos+off>=len)
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            c = str.at(start_pos+off);
            if(!isdigit(c))
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }

            off++;
            for(;start_pos+off<len;off++)
            {
                c = str.at(start_pos+off);
                if(c=='.')
                {
                    if(hasPoint)
                    {
                        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                        //err
                    }else
                    {
                        hasPoint=true;
                        continue;
                    }
                }

                if(!isdigit(c))
                {
                    break; // digit end.
                }

                //is digit ok, find next
            }
            if(start_pos+off<len)
            {
                Element ele;
                ele.lineNo = lineNo;
                ele.linePos = start_pos-linePos;
                ele.start_pos=start_pos;
                ele.end_pos = start_pos+(off+1 -1-1);
                ele.str = str.substr(start_pos,off+1 -1);
                if(hasPoint) ele.type = DOUBLE;
                else         ele.type = NUMBER;

                elements.push_back(ele);
                start_pos += off -1;
                continue;
            }else
            {
                throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
                //err
            }
        }


        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
        //err

    }//for


    // simple check
    if(elements.size()<2)
    {
        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
    }
    Element &ele = elements.back();
    if(ele.str != "}")
    {
        throw SmyExp::PasserStringErr("line:%d, element char pos:%d", lineNo,start_pos-linePos);
    }

    return true;

}

Element & eleAt(vector<Element> &elements, int pos)
{
    return elements.at(pos);
}


class EleList
{
public:
    EleList(list<Element> &eleList):elements(eleList)
    {

    }

    list<Element> &elements;
    Element& operator [] (int pos)
    {
        for(auto &x:elements)
        {
            pos--;
            if(pos==-1)
            {
                return x;
            }
        }

        throw SmyExp::ArrayPosErr("");

    }
    int size()
    {
        return elements.size();
    }
    void remove(int pos)
    {
        for (list<Element>::iterator Itor = elements.begin(); Itor != elements.end(); Itor++)
        {
            pos--;
            if(pos==-1)
            {
                trashList.push_back(*Itor);
                elements.erase(Itor);
                return;
            }
        }

        throw SmyExp::ArrayPosErr("");

    }

    vector<Element> trashList;
};


static int passerObject(Value& obj, EleList &els, int pos)
{
    int cur = pos;
    int size= els.size();

    bool can_not_end=false;

    while(cur<size)
    {
        //find } or key-value
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerObject, find } err, (cur>=size) err, cur=%d, size=%d\n",cur,size);
        }

        if(els[cur].type == BraceR)
        {
            if(can_not_end)
            {
                throw SmyExp::PasserElementsErr("passerObject, find } err, type is not }, type=%s\n",typeStr[els[cur].type].data());
            }else
            {
                return cur-pos+1;
                //end ok
            }

        }

        //find key
        if(els[cur].type != STRING)
        {
            throw SmyExp::PasserElementsErr("passerObject, find key err,type is not key, type=%s\n",typeStr[els[cur].type].data());
        }

        string key = els[cur].toKey();

        cur++;

        //find :
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerObject, find : err, (cur>=size)  err, cur=%d, size=%d\n",cur,size);
        }

        if(els[cur].type != Colon)
        {
            throw SmyExp::PasserElementsErr("passerObject, find : err,type is not key, type=%s\n",typeStr[els[cur].type].data());
        }

        cur++;

        //find value
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerObject, find value err, (cur>=size)  err, cur=%d, size=%d\n",cur,size);
        }

        switch(els[cur].type)
        {
        case BraceL:
        {
            cur++;

            Value value=NUL;
            int n = passerObject(value, els, cur);
            cur+=n;
            obj[key] = value;
        }break;
        case BracketL:
        {
            cur++;

            Value value=NUL;
            int n = passerArray(value, els, cur);
            cur+=n;
            obj[key] = value;
        }break;
            //value
        case NUMBER:
        case DOUBLE:
        case BOOL:
        case STRING:
        case NUL:
        {
            // current is value

            Value value=NUL;
            int n = paseValue(value, els, cur);
            cur+=n;
            obj[key] = value;

        }break;
        default:
        {
            throw SmyExp::PasserElementsErr("passerObject, find value err,type is not value, type=%s\n",typeStr[els[cur].type].data());
        }break;
        }//switch

        //find ,
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerObject, find , err, (cur>=size)  err, cur=%d, size=%d\n",cur,size);
        }
        if(els[cur].type == Comma)// seperate more .
        {
            cur++;
            can_not_end = true;
        }else
        {
            can_not_end = false;
        }

    }

    throw SmyExp::PasserElementsErr("passerObject, end err, deal nothing, cur=%d, size=%d\n",cur,size);

}

static int passerArray(Value& obj, EleList &els, int pos)
{
    int cur = pos;
    int size= els.size();

    bool can_not_end=false;

    while(cur<size)
    {
        //find ] or value
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerArray, find ] err, (cur>=size) err, cur=%d, size=%d\n",cur,size);
        }

        if(els[cur].type == BracketR)
        {
            if(can_not_end)
            {
                throw SmyExp::PasserElementsErr("passerArray, find ] err, type is not ], type=%s\n",typeStr[els[cur].type].data());
            }else
            {
                return cur-pos+1;
                //end ok
            }

        }

        switch(els[cur].type)
        {
        case BraceL:
        {
            cur++;

            Value value=NUL;
            int n = passerObject(value, els, cur);
            cur+=n;
            obj.append(value);
        }break;
        case BracketL:
        {
            cur++;

            Value value=NUL;
            int n = passerArray(value, els, cur);
            cur+=n;
            obj.append(value);
        }break;
            //value
        case NUMBER:
        case DOUBLE:
        case BOOL:
        case STRING:
        case NUL:
        {
            // current is value

            Value value=NUL;
            int n = paseValue(value, els, cur);
            cur+=n;
            obj.append(value);

        }break;
        default:
        {
            throw SmyExp::PasserElementsErr("passerArray, find value err,type is not value, type=%s\n",typeStr[els[cur].type].data());
        }break;
        }//switch

        //find ,
        if(cur>=size)
        {
            throw SmyExp::PasserElementsErr("passerArray, find , err, (cur>=size)  err, cur=%d, size=%d\n",cur,size);
        }
        if(els[cur].type == Comma)// seperate more .
        {
            cur++;
            can_not_end = true;
        }else
        {
            can_not_end = false;
        }

    }

    throw SmyExp::PasserElementsErr("passerArray, end err, deal nothing, cur=%d, size=%d\n",cur,size);
}
static int paseValue(Value& obj, EleList &els, int pos)
{
    int cur = pos;
    int size= els.size();

    //find value
    if(cur>=size)
    {
        throw SmyExp::PasserElementsErr("paseValue, find value err, (cur>=size) err, cur=%d, size=%d\n",cur,size);
    }

    switch(els[cur].type)
    {
    //value
    case NUMBER:
    {
        obj = els[cur].toNumber();
        cur++;
    }break;
    case DOUBLE:
    {
        obj = els[cur].toDouble();
        cur++;
    }break;
    case BOOL:
    {
        obj = els[cur].toBool();
        cur++;
    }break;
    case STRING:
    {
        obj = els[cur].toString();
        cur++;
    }break;
    case NUL:
    {
        obj = NUL;
        cur++;
    }break;
    default:
    {
        throw SmyExp::PasserElementsErr("paseValue, find value err,type is not value, type=%s\n",typeStr[els[cur].type].data());
    }break;
    }//switch

    return cur-pos;
}

static int passerElements2Json(Value &root, EleList &els, int pos=0)
{
    root = NUL;

    if(pos < 0) return 0;
    if(pos >= els.size()) return 0;

    switch(els[pos].type)
    {
    case BraceL:
    {
        int n = passerObject(root, els, 1);
        return n+1;
    }break;
    case BracketL:
    {
        int n = passerArray(root, els, 1);
        return n+1;
    }break;
        //value
    case NUMBER:
    case DOUBLE:
    case BOOL:
    case STRING:
    case NUL:
    {
        int n = paseValue(root, els, 0);
        return n;

    }break;
    default:
    {
        throw SmyExp::PasserElementsErr("json string start err, type=%s\n",typeStr[els[0].type].data());
    }break;
    }//switch

    throw SmyExp::PasserElementsErr("passer, end err, deal nothing, cur=%d, size=%d\n",pos ,els.size());

}


//throw SmyExp::MyExp &e
bool passer(Value &root, string &in_str, int *p_dealStrLen=NULL)
{
    try{

        root = SmyJson::NUL;

        int len=0;
        if(p_dealStrLen==NULL) p_dealStrLen = &len;

        stringstream  stm_in(in_str);

        list<Element> stm_eles;
        EleList stm_eleList(stm_eles);

        bool ok = SmyJson::passerStream2Elements(stm_in, stm_eles, *p_dealStrLen);
        if(!ok) return false;

        int n = SmyJson::passerElements2Json(root,stm_eleList);
        if(n==stm_eleList.size()) return true;

        return false;
    }catch(SmyExp::SmyExp &e)
    {
        return false;
    }
}

//throw SmyExp::MyExp &e
bool passer(Value &root, stringstream  &stm_in, int *p_dealStrLen=NULL)
{
    try{

        int len=0;
        if(p_dealStrLen==NULL) p_dealStrLen = &len;

        list<Element> stm_eles;
        EleList stm_eleList(stm_eles);

        bool ok = SmyJson::passerStream2Elements(stm_in, stm_eles, *p_dealStrLen);
        if(!ok) return false;

        int n = SmyJson::passerElements2Json(root,stm_eleList);
        if(n==stm_eleList.size()) return true;

        return false;
    }catch(SmyExp::SmyExp &e)
    {
        return false;
    }
}

//throw SmyExp::MyExp &e
static bool passer(Value &root, ifstream   &stm_in, int *p_dealStrLen=NULL)
{
    try{

        int len=0;
        if(p_dealStrLen==NULL) p_dealStrLen = &len;

        list<Element> stm_eles;
        EleList stm_eleList(stm_eles);

        bool ok = SmyJson::passerStream2Elements(stm_in, stm_eles, *p_dealStrLen);
        if(!ok) return false;

        int n = SmyJson::passerElements2Json(root,stm_eleList);
        if(n==stm_eleList.size()) return true;

        return false;
    }catch(SmyExp::SmyExp &e)
    {
        return false;
    }

}

//throw SmyExp::MyExp &e
static bool passerFile(Value &root, const string &filename, int *p_dealStrLen=NULL)
{
    ifstream ifs;
    ifs.open(filename);
    if(!ifs.is_open()) return false;

    bool ok = passer(root, ifs,p_dealStrLen);
    ifs.close();
    return ok;

}

static bool writeFile(Value &root, const string &filename, const string &tagStr="  ", int combine=10, int tab=0)
{
    ofstream ofs;
    ofs.open(filename);
    if(!ofs.is_open()) return false;

    string s = root.serialize("\n", tagStr, combine, tab);
    ofs.write(s.data(), s.length());
    ofs.close();
    return true;

}

bool writeFile(Value &root, ofstream &ofs)
{
    string s = root.serialize();
    ofs.write(s.data(), s.length());
    return true;
}

};


#endif // SMYJSON_H
