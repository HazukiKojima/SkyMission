#include "JsonBenchmark.h"
#include "../../External/nlohmann/json.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace Engine { namespace {
using Fn=int(__cdecl*)(const unsigned char*,int,const unsigned char*,int,unsigned char*,int);
struct D { std::string n; std::filesystem::path p; std::vector<unsigned char> b; };
int Env(const wchar_t* n,int d) { wchar_t v[32]{}; DWORD l=GetEnvironmentVariableW(n,v,32); if(!l || l>=32)return d; int x=_wtoi(v); return x>0?x:d; }
std::vector<unsigned char> Read(const std::filesystem::path& p) { std::ifstream f(p,std::ios::binary); f.seekg(0,std::ios::end); auto n=f.tellg(); std::vector<unsigned char>b((size_t)n); f.seekg(0); f.read((char*)b.data(),n); return b; }
double Nlohmann(const D& d,int n,int& len) { auto s=std::chrono::steady_clock::now(); for(int i=0;i<n;++i) { auto j=nlohmann::json::parse(d.b.begin(),d.b.end()); len=(int)j.at("target").get<std::string>().size(); } return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-s).count()/n; }
double Redox(Fn f,const D& d,int n,int& len) { std::array<unsigned char,4096>o{}; constexpr std::array<unsigned char,6>k={'t','a','r','g','e','t'}; auto s=std::chrono::steady_clock::now(); for(int i=0;i<n;++i)len=f(d.b.data(),(int)d.b.size(),k.data(),6,o.data(),(int)o.size()); return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-s).count()/n; }
}
int RunJsonBenchmark() { int w=Env(L"REDOX_BENCH_WARMUP",20),n=Env(L"REDOX_BENCH_ITERATIONS",1000),r=Env(L"REDOX_BENCH_ROUNDS",5); wchar_t x[MAX_PATH]{}; DWORD l=GetModuleFileNameW(nullptr,x,MAX_PATH); auto e=std::filesystem::path(x,x+l).parent_path(); HMODULE m=LoadLibraryW((e/L"REDoxWrapper.dll").c_str()); if(!m) return -1; auto f=(Fn)GetProcAddress(m,"MyRedoxGetString"); std::ofstream csv(e/L"JsonBenchmarkResults.csv"); csv<<"Dataset,JSONBytes,Library,Round,Iterations,AverageUs,Speedup,Result\n"; for(auto [name,file]:std::array{std::pair{"Small",L"Small.json"},std::pair{"Medium",L"Medium.json"},std::pair{"Large",L"Large.json"}}) { D d{name,e/L"..\\..\\Assets\\Benchmark"/file}; d.b=Read(d.p); int a=0,b=0; for(int i=0;i<w;++i){Nlohmann(d,1,a);Redox(f,d,1,b);} std::vector<double> nv,rv; for(int q=1;q<=r;++q){double nn=Nlohmann(d,n,a),rr=Redox(f,d,n,b); if(a!=b){FreeLibrary(m);return -2;} nv.push_back(nn);rv.push_back(rr);csv<<name<<","<<d.b.size()<<",nlohmann/json,"<<q<<","<<n<<","<<nn<<",,,Success\n"<<name<<","<<d.b.size()<<",RE:Dox,"<<q<<","<<n<<","<<rr<<",,,Success\n";} std::sort(nv.begin(),nv.end());std::sort(rv.begin(),rv.end());double nm=nv[r/2],rm=rv[r/2];csv<<name<<","<<d.b.size()<<",nlohmann/json median,,,"<<nm<<","<<(rm/nm)<<",,Success\n"<<name<<","<<d.b.size()<<",RE:Dox median,,,"<<rm<<","<<(nm/rm)<<",,Success\n";} FreeLibrary(m); return 0; } }
