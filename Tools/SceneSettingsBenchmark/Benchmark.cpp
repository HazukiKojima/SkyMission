#include <Windows.h>
#include "Game/Scene/FlightSceneLoader.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Engine/Interop/Redox/RedoxBridge.h"
#undef GetProcAddress
#undef LoadLibraryW
#include "json.hpp"
#include <chrono>
#include <bit>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unordered_map>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
using Json = nlohmann::json;
using Handle = std::intptr_t;
bool useNative = false, recording = false;
std::ofstream trace;
size_t calls = 0, documentsCreated = 0;
HMODULE redoxModule = nullptr;
struct Field { std::string name; int length; };
using Layout = std::vector<Field>;
#include "Layouts.inc"
struct Slot { int offset, length; };
const auto& GetLayoutIndices() {
static const auto indices = [] {
    std::unordered_map<std::string, std::unordered_map<std::string, Slot>> result;
    for (const auto& [kind, fields] : GetLayouts()) {
        int offset = 0;
        for (const auto& field : fields) {
            result[kind].emplace(field.name, Slot{offset, field.length}); offset += field.length;
        }
    }
    return result;
}();
return indices;
}
struct Document { Json root; std::unordered_map<std::string, std::vector<float>> cache; };
std::unordered_map<Handle, Document> documents;
Handle nextHandle = 0;

void Record(const std::string& operation, int result, const void* data = nullptr, size_t bytes = 0) {
    if (!recording) return;
    ++calls;
    trace << operation << ' ' << result;
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) trace << ' ' << unsigned(p[i]);
    trace << '\n';
}
template<class F> F Real(const char* name) {
    auto p = ::GetProcAddress(redoxModule, name);
    if (!p) throw std::runtime_error(std::string("Missing production export: ") + name);
    return reinterpret_cast<F>(p);
}
const Json* Find(Handle h, const unsigned char* key, int length) {
    const auto it = documents.find(h);
    if (it == documents.end()) return nullptr;
    const std::string path(reinterpret_cast<const char*>(key), length);
    const Json& root = it->second.root;
    if (path.starts_with('/')) {
        Json::json_pointer pointer(path);
        return root.contains(pointer) ? &root.at(pointer) : nullptr;
    }
    auto value = root.find(path);
    return value == root.end() ? nullptr : &*value;
}
bool ReadField(const Json& value, int count, std::vector<float>& values) {
    if (count != 1 && (!value.is_array() || value.size() != size_t(count))) return false;
    for (int i = 0; i < count; ++i) {
        const Json& item = count == 1 ? value : value.at(i);
        if (!item.is_number()) return false;
        float v = item.get<float>();
        if (!std::isfinite(v)) return false;
        values.push_back(v);
    }
    return true;
}
int NativeParameters(Handle h, const std::string& kind, float* output, int capacity) {
    auto& doc = documents.at(h);
    auto cached = doc.cache.find(kind);
    if (cached == doc.cache.end()) {
        std::vector<float> values;
        const auto& layout = GetLayouts().at(kind);
        if (kind == "RetroFilm") {
            // The production retro-film reader uses fixed nested property lookups.
            for (const auto& field : layout) {
                if (!doc.root.contains(Json::json_pointer(field.name))) return -3;
                if (!ReadField(doc.root.at(Json::json_pointer(field.name)), field.length, values)) return -5;
            }
            values.push_back(0); values.push_back(0);
        } else {
            // Same one-pass field placement algorithm as NumericParameterLayout.
            const auto& indices = GetLayoutIndices().at(kind);
            int size = 0;
            for (const auto& field : layout) size += field.length;
            values.resize(size);
            int found = 0;
            for (auto it = doc.root.begin(); it != doc.root.end(); ++it) {
                auto slot = indices.find(it.key());
                if (slot == indices.end()) continue;
                const int count = slot->second.length;
                const auto& value = it.value();
                if (count != 1 && (!value.is_array() || value.size() != size_t(count))) return -5;
                for (int i = 0; i < count; ++i) {
                    const auto& item = count == 1 ? value : value.at(i);
                    if (!item.is_number()) return -5;
                    float number = item.get<float>();
                    if (!std::isfinite(number)) return -5;
                    values[slot->second.offset + i] = number;
                }
                if (++found == int(layout.size())) break;
            }
            if (found != int(layout.size())) return -5;
        }
        if (values.size() != size_t(capacity)) return -5;
        // Production's retro-film path has no typed-value cache.
        if (kind == "RetroFilm") { std::copy(values.begin(), values.end(), output); return capacity; }
        cached = doc.cache.emplace(kind, std::move(values)).first;
    }
    if (cached->second.size() != size_t(capacity)) return -5;
    std::copy(cached->second.begin(), cached->second.end(), output);
    return capacity;
}
int __cdecl Create(const unsigned char* input, int length, Handle* output) {
    int result;
    if (!useNative) result = Real<int(__cdecl*)(const unsigned char*, int, Handle*)>("MyRedoxCreateDocument")(input, length, output);
    else {
        try { auto root = Json::parse(input, input + length); *output = ++nextHandle;
            documents.emplace(*output, Document{std::move(root), {}}); result = 0;
        } catch (...) { *output = 0; result = -2; }
    }
    if (recording && result == 0) ++documentsCreated;
    Record("Create", result, input, recording ? size_t(length) : 0);
    return result;
}
int __cdecl Release(Handle h) {
    int result = useNative ? (documents.erase(h) ? 0 : -1)
        : Real<int(__cdecl*)(Handle)>("MyRedoxReleaseDocument")(h);
    Record("Release", result); return result;
}
int __cdecl GetNumbers(Handle h, const unsigned char* key, int length, float* output, int capacity) {
    int result;
    if (!useNative) result = Real<int(__cdecl*)(Handle,const unsigned char*,int,float*,int)>("MyRedoxGetFloatArrayFromDocument")(h,key,length,output,capacity);
    else {
        auto value = Find(h,key,length);
        if (!value) result = -3;
        else {
            std::vector<float> values;
            if (value->is_array()) {
                result = int(value->size());
                if (result > capacity) result = -5;
                else for (const auto& item : *value) if (!ReadField(item,1,values)) { result = -5; break; }
            } else result = ReadField(*value,1,values) ? 1 : -5;
            if (result >= 0) std::copy(values.begin(),values.end(),output);
        }
    }
    if (recording) Record("Number:" + std::string(reinterpret_cast<const char*>(key),length),result,output,result > 0 ? result*sizeof(float) : 0);
    return result;
}
int __cdecl GetString(Handle h,const unsigned char* key,int length,unsigned char* output,int capacity) {
    int result;
    if (!useNative) result = Real<int(__cdecl*)(Handle,const unsigned char*,int,unsigned char*,int)>("MyRedoxGetStringFromDocument")(h,key,length,output,capacity);
    else {
        const auto* value = Find(h,key,length);
        if (!value) result = -3;
        else if (!value->is_string()) result = -5;
        else { const auto& text = value->get_ref<const std::string&>(); result = int(text.size());
            if (result > capacity) result = -5; else std::memcpy(output,text.data(),result); }
    }
    if (recording) Record("String:"+std::string(reinterpret_cast<const char*>(key),length),result,output,result > 0 ? result : 0); return result;
}
int __cdecl GetLength(Handle h,const unsigned char* key,int length) {
    int result;
    if (!useNative) result = Real<int(__cdecl*)(Handle,const unsigned char*,int)>("MyRedoxGetArrayLengthFromDocument")(h,key,length);
    else { const auto* value = Find(h,key,length); result = !value ? -3 : value->is_array() ? int(value->size()) : -5; }
    if (recording) Record("Length:"+std::string(reinterpret_cast<const char*>(key),length),result); return result;
}
int Parameters(const char* kind,Handle h,float* output,int capacity) {
    const std::string name = std::string("MyRedoxGet") + kind + "ParametersFromDocument";
    int result = useNative ? NativeParameters(h,kind,output,capacity)
        : Real<int(__cdecl*)(Handle,float*,int)>(name.c_str())(h,output,capacity);
    Record(kind,result,output,result > 0 ? result*sizeof(float) : 0); return result;
}
#define PARAMETER_FUNCTION(kind) int __cdecl kind(Handle h,float* p,int n) { return Parameters(#kind,h,p,n); }
PARAMETER_FUNCTION(AircraftAI)
PARAMETER_FUNCTION(AircraftFlight)
PARAMETER_FUNCTION(Ocean)
PARAMETER_FUNCTION(Sky)
PARAMETER_FUNCTION(RetroFilm)
}
FARPROC WINAPI SceneBenchmarkGetProcAddress(HMODULE module,LPCSTR name) {
    redoxModule = module;
    if (!useNative && !recording) return ::GetProcAddress(module, name);
    const std::string key(name);
#define ENTRY(api,fn) if (key == api) return reinterpret_cast<FARPROC>(&fn)
    ENTRY("MyRedoxCreateDocument",Create);
    ENTRY("MyRedoxReleaseDocument",Release);
    ENTRY("MyRedoxGetFloatArrayFromDocument",GetNumbers);
    ENTRY("MyRedoxGetStringFromDocument",GetString);
    ENTRY("MyRedoxGetArrayLengthFromDocument",GetLength);
#define PARAMETER_ENTRY(kind) ENTRY("MyRedoxGet" #kind "ParametersFromDocument",kind)
    PARAMETER_ENTRY(AircraftAI);
    PARAMETER_ENTRY(AircraftFlight);
    PARAMETER_ENTRY(Ocean);
    PARAMETER_ENTRY(Sky);
    PARAMETER_ENTRY(RetroFilm);
    throw std::runtime_error("Unexpected API: " + key);
}
HMODULE WINAPI SceneBenchmarkLoadLibraryW(LPCWSTR name) {
    return useNative ? reinterpret_cast<HMODULE>(1) : ::LoadLibraryW(name);
}

int main(int argc,char** argv) try {
    if (argc != 5) throw std::runtime_error("Usage: Benchmark repository output redox|nlohmann processIndex");
    useNative = std::string(argv[3]) == "nlohmann";
    const auto root = std::filesystem::absolute(argv[1]) / "Assets";
    const Game::SceneSourcePaths paths{
        .environment=root/"Environment/environment.json",
        .playerAircraft=root/"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
        .enemyAircraft=root/"Aircraft/F4U4Corsair/F4U4Corsair.json",
        .retroFilm=root/"Environment/retroFilm.json", .ships=root/"Ships/ships.json",
        .mission=root/"Combat/mission.json", .bomb=root/"Ammunition/Bomb/ammunition.json"};
    const Game::FlightSceneLoader loader;
    using Clock = std::chrono::steady_clock;
    // Includes disk reads, parsing, all lookups, validation and loader-owned cache destruction.
    auto run = [&] { auto definition = loader.Load(paths);
        // Match CombatAudio::Initialize's document-scoped settings read, excluding audio decoding.
        Game::AudioSettings audio;
        Engine::JsonDocumentCache audioDocuments;
        audio.Load(Engine::JsonAssetReader{audioDocuments}, root / "Sound/audio.json");
        if (definition.ships.empty() || definition.cinematics.empty()) throw std::runtime_error("Incomplete scene definition");
    };
    auto before = Clock::now(); run();
    const double firstUs = std::chrono::duration<double,std::micro>(Clock::now()-before).count();
    const auto stem = std::string(argv[3])+"-"+argv[4];
    trace.open(std::filesystem::path(argv[2])/(stem+"-trace.txt"));
    recording = true; run(); recording = false; trace.close();
    for (int warmup=0; warmup<5; ++warmup) run();
    std::ofstream csv(std::filesystem::path(argv[2])/(stem+".csv"));
    csv << "backend,process,sample,iterations,microseconds_per_load\n" << std::setprecision(12);
    for(int sample=0;sample<7;++sample) {
        before = Clock::now(); int count=0; double elapsed;
        do {run(); ++count; elapsed=std::chrono::duration<double>(Clock::now()-before).count();} while(elapsed<0.15);
        csv << argv[3] << ',' << argv[4] << ',' << sample << ',' << count << ',' << elapsed*1e6/count << '\n';
    }
    std::ofstream info(std::filesystem::path(argv[2])/(stem+"-info.txt"));
    info << "first_load_us=" << firstUs << "\ntrace_calls=" << calls << "\ndocuments=" << documentsCreated << '\n';
    std::cout << stem << ": first=" << firstUs << " us; documents=" << documentsCreated << "; calls=" << calls << '\n';
    return 0;
} catch(const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
