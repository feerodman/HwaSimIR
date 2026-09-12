#pragma once
#include <opencv2/core.hpp>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <iomanip>

// Reuse the linked OpenCV JSON tree. The lexical gate preserves boolean types,
// rejects duplicate keys/trailing input/nonfinite numbers before FileStorage.
namespace IRJson {
class Document {
public:
    cv::FileStorage tree;
    std::map<std::string,char> types;
    std::string hash;
    void load(const std::string& file) {
        std::ifstream in(file.c_str(),std::ios::binary);
        if(!in) throw std::runtime_error("file_missing: "+file);
        std::ostringstream buf;buf<<in.rdbuf();text=buf.str();pos=0;normalized.clear();types.clear();
        std::uint64_t h=14695981039346656037ULL;
        for(unsigned char c:text){h^=c;h*=1099511628211ULL;}
        std::ostringstream hex;hex<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<h;hash=hex.str();
        if(text.size()>=3 && text.substr(0,3)=="\xef\xbb\xbf")pos=3;
        value("",0);space();if(pos!=text.size())fail("trailing_input");
        tree.open(normalized,cv::FileStorage::READ|cv::FileStorage::MEMORY|cv::FileStorage::FORMAT_JSON);
        if(!tree.isOpened() || !tree.root().isMap())fail("root_must_be_object");
    }
    cv::FileNode at(const std::string& path) const {
        cv::FileNode n=tree.root();std::istringstream ss(path);std::string key;
        while(std::getline(ss,key,'.')){if(!n.isMap())return cv::FileNode();n=n[key];}return n;
    }
    bool has(const std::string& p) const {return types.count(p)!=0;}
    void require(const std::string& p,char t) const {
        auto i=types.find(p);if(i==types.end())throw std::runtime_error("missing: "+p);
        if(i->second!=t)throw std::runtime_error("wrong_type: "+p);
    }
    double number(const std::string& p,double lo,double hi) const {
        require(p,'n');double v=double(at(p));
        if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error("out_of_range_or_nonfinite: "+p);return v;
    }
    int integer(const std::string& p,int lo,int hi) const {
        double v=number(p,lo,hi);if(v!=std::floor(v))throw std::runtime_error("expected_integer: "+p);return int(v);
    }
    bool boolean(const std::string& p) const {require(p,'b');return int(at(p))!=0;}
    std::string string(const std::string& p) const {require(p,'s');return std::string(at(p));}
private:
    std::string text,normalized;size_t pos=0;
    void fail(const char* why) const {throw std::runtime_error(std::string("invalid_json:")+why+" byte="+std::to_string(pos));}
    void space(){while(pos<text.size() && (text[pos]==' '||text[pos]=='\n'||text[pos]=='\r'||text[pos]=='\t'))++pos;}
    char peek(){space();return pos<text.size()?text[pos]:'\0';}
    void take(char c){if(peek()!=c)fail("unexpected_token");normalized+=text[pos++];}
    std::string quoted(bool key=false){
        take('"');std::string s;
        while(pos<text.size()){
            char c=text[pos++];normalized+=c;if(c=='"')return s;
            if(static_cast<unsigned char>(c)<32)fail("string_control");
            if(c=='\\'){
                if(key)fail("escaped_schema_key_unsupported");
                if(pos>=text.size())fail("escape");char e=text[pos++];
                if(e=='/'){normalized.back()='/';continue;} // OpenCV 4.4 rejects JSON's optional escaped slash.
                normalized+=e;
                if(e=='u'){for(int j=0;j<4;++j){if(pos>=text.size()||!std::isxdigit(static_cast<unsigned char>(text[pos])))fail("unicode_escape");normalized+=text[pos++];}}
                else if(std::string("\"\\/bfnrt").find(e)==std::string::npos)fail("escape");
            }else s+=c;
        }fail("unclosed_string");return s;
    }
    void value(const std::string& path,int depth){
        if(depth>64)fail("depth_limit");char c=peek();char kind=c=='{'?'o':c=='['?'a':c=='"'?'s':(c=='t'||c=='f')?'b':c=='n'?'z':'n';
        if(types.count(path))fail("duplicate_key");types[path]=kind;
        if(c=='{'){
            take('{');if(peek()=='}'){take('}');return;}
            for(;;){std::string k=quoted(true);take(':');value(path.empty()?k:path+"."+k,depth+1);if(peek()=='}'){take('}');break;}take(',');}
        }else if(c=='['){
            take('[');int i=0;if(peek()==']'){take(']');return;}
            for(;;){value(path+"["+std::to_string(i++)+"]",depth+1);if(peek()==']'){take(']');break;}take(',');}
        }else if(c=='"'){quoted();}
        else if(c=='t'||c=='f'||c=='n'){
            const std::string token=c=='t'?"true":c=='f'?"false":"null";
            if(text.compare(pos,token.size(),token)!=0)fail("literal");pos+=token.size();normalized+=c=='t'?"1":c=='f'?"0":"\"null\"";
        }else{
            space();size_t b=pos;if(pos<text.size()&&text[pos]=='-')++pos;
            if(pos>=text.size())fail("number");
            if(text[pos]=='0')++pos;else{if(text[pos]<'1'||text[pos]>'9')fail("number");while(pos<text.size()&&std::isdigit(static_cast<unsigned char>(text[pos])))++pos;}
            if(pos<text.size()&&text[pos]=='.'){++pos;size_t d=pos;while(pos<text.size()&&std::isdigit(static_cast<unsigned char>(text[pos])))++pos;if(pos==d)fail("fraction");}
            if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;size_t d=pos;while(pos<text.size()&&std::isdigit(static_cast<unsigned char>(text[pos])))++pos;if(pos==d)fail("exponent");}
            const std::string num=text.substr(b,pos-b);double v=std::strtod(num.c_str(),nullptr);if(!std::isfinite(v))fail("nonfinite");normalized+=num;
        }
    }
};
}
