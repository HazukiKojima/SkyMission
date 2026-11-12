#include "Engine/Interop/Redox/RedoxBridge.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Game/Settings/GameplaySettings.h"
#include <iostream>

namespace fs = std::filesystem;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("Usage: CacheChecks output-directory");
    const auto originalDirectory = fs::current_path();
    struct RestoreDirectory { fs::path value; ~RestoreDirectory() { fs::current_path(value); } } restore{originalDirectory};
    const auto fixture = fs::absolute(argv[1]) / "cache-checks";
    fs::create_directories(fixture / "one");
    fs::create_directories(fixture / "two");
    const auto first = fixture / "one/settings.json", second = fixture / "two/settings.json";
    std::ofstream(first) << R"({"value":1})";
    std::ofstream(second) << R"({"value":2})";
    Engine::JsonDocumentCache documents;
    Engine::JsonAssetReader reader(documents);
    const auto handle = documents.Get(first).GetHandle();
    Require(documents.Get(first).GetHandle() == handle, "Absolute last-path lookup lost document identity");
    Require(documents.Get(first.parent_path() / "./settings.json").GetHandle() == handle, "Normalized alias duplicated document");
    Require(reader.ResolveFloat(second,"value") == 2 && documents.Get(first).GetHandle() == handle,
        "Inserting another document invalidated cached reference");
    std::ofstream(first) << R"({"value":3})";
    Require(reader.ResolveFloat(first,"value") == 1, "In-flight cache unexpectedly reloaded changed file");
    Engine::JsonDocumentCache fresh;
    Require(Engine::JsonAssetReader(fresh).ResolveFloat(first,"value") == 3, "New cache did not reload changed file");
    fs::current_path(first.parent_path());
    Require(reader.ResolveFloat("settings.json","value") == 1, "Relative alias missed existing document");
    fs::current_path(second.parent_path());
    Require(reader.ResolveFloat("settings.json","value") == 2, "Relative cache ignored working-directory change");
    Require(reader.ResolveFloat(first,"value") == 1, "Relative access corrupted last absolute lookup");
    const auto audio = fixture / "audio.json";
    std::ofstream(audio) << R"({"flightVolume":0.25,"flightFile":"test.wav"})";
    Game::AudioSettings settings;
    settings.Load(reader,audio);
    Require(settings.flightVolume == 0.25f && settings.flightFile == "test.wav" && settings.gunVolume == 0.42f,
        "Cached partial audio settings lost values/defaults");
    std::ofstream(audio) << R"({"flightVolume":2})";
    bool rejected = false;
    try { Engine::JsonDocumentCache invalid; settings.Load(Engine::JsonAssetReader(invalid),audio); }
    catch (const std::exception& error) { rejected = std::string(error.what()).find("flightVolume") != std::string::npos; }
    Require(rejected, "Cached audio path lost validation or field error context");
    Engine::JsonDocumentCache moved(std::move(documents));
    Require(Engine::JsonAssetReader(moved).ResolveFloat(first,"value") == 1, "Moving cache lost parsed documents");
    Require(Engine::JsonAssetReader(documents).ResolveFloat(first,"value") == 3, "Moved-from cache retained another cache's pointer");
    Engine::JsonDocumentCache assigned;
    assigned = std::move(moved);
    Require(Engine::JsonAssetReader(assigned).ResolveFloat(first,"value") == 1, "Move assignment lost parsed documents");
    Require(Engine::JsonAssetReader(moved).ResolveFloat(first,"value") == 3, "Move-assigned source retained stale pointer");
    std::cout << "PASS: document identity, path aliases, insertion stability, cache lifetime, cwd changes, audio defaults/errors\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
