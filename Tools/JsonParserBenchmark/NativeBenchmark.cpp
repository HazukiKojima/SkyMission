#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <span>
#include <unordered_map>
#include "json.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
namespace {
	volatile double g_checksum = 0;
	constexpr int kSamples = 7;
	constexpr double kSampleSeconds = 0.1;
	struct Dataset {
		std::string name;
		std::vector<std::string> documents;
		size_t bytes = 0;
	};
	struct Bridge {
		using Create = int(__cdecl*)(const char*, int, intptr_t*);
		using Release = int(__cdecl*)(intptr_t);
		using ReadAi = int(__cdecl*)(intptr_t, float*, int);
		using ReadNumber = int(__cdecl*)(intptr_t, const char*, int, float*);
		using ReadFloatArray = int(__cdecl*)(intptr_t, const char*, int, float*, int);
		using Collect = void(__cdecl*)();
		HMODULE module;
		Create create, createUtf8;
		Release release;
		ReadAi readAi;
		ReadNumber readNumber;
		ReadFloatArray readFloatArray = nullptr;
		Collect collect;
		explicit Bridge(const fs::path& path, bool gameApi = false, bool utf8Only = false) {
			module = LoadLibraryW(path.c_str());
			if (!module) throw std::runtime_error("Native AOT DLL load failed: " + path.string());
			create = Load<Create>(gameApi ? "MyRedoxCreateDocument" : (utf8Only ? "CreateUtf8" : "Create"));
			createUtf8 = gameApi || utf8Only ? create : Load<Create>("CreateUtf8");
			release = Load<Release>(gameApi ? "MyRedoxReleaseDocument" : "Release");
			readAi = Load<ReadAi>(gameApi ? "MyRedoxGetAircraftAIParametersFromDocument" : "ReadAi");
			readNumber = gameApi ? nullptr : Load<ReadNumber>("ReadNumber");
			if (gameApi) readFloatArray = Load<ReadFloatArray>("MyRedoxGetFloatArrayFromDocument");
			collect = Load<Collect>("Collect");
		}
		int ReadSingle(intptr_t handle, const char* key, int length, float* output) const {
			return readFloatArray ? readFloatArray(handle, key, length, output, 1)
								  : readNumber(handle, key, length, output);
		}
		template <typename T> T Load(const char* name) const {
			const auto address = GetProcAddress(module, name);
			if (!address) throw std::runtime_error(std::string("Missing export: ") + name);
			return reinterpret_cast<T>(address);
		}
	};

	using FieldIndices = std::unordered_map<std::string, size_t>;
	// 一括走査・有限float検証・文書単位の型付きキャッシュを.NET側とそろえる
	class MatchedNlohmannDocument {
	  public:
		explicit MatchedNlohmannDocument(const std::string& input)
			: m_document(nlohmann::json::parse(input)) {}
		__declspec(noinline) int ReadAi(
			const std::vector<std::string>& fields, const FieldIndices& indices, std::span<float> output) {
			if (output.size() != 30) return -1;
			auto found = m_cache.find(&fields);
			if (found == m_cache.end()) {
				if (!m_document.is_object()) return -3;
				std::array<float, 30> values{};
				std::array<bool, 30> seen{};
				size_t count = 0;
				for (auto item = m_document.begin(); item != m_document.end(); ++item) {
					const auto field = indices.find(item.key());
					if (field == indices.end() || seen[field->second]) continue;
					if (!item.value().is_number()) return -5;
					const float number = item.value().get<float>();
					if (!std::isfinite(number)) return -5;
					values[field->second] = number;
					seen[field->second] = true;
					if (++count == fields.size()) break;
				}
				if (count != fields.size()) return -5;
				found = m_cache.emplace(&fields, values).first;
			}
			// 非インライン化で出力のコピーを省略させず、全方式で同じ30個を消費する
			std::copy(found->second.begin(), found->second.end(), output.begin());
			return 0;
		}
		float ReadNumber(const std::string& key) const {
			const auto& value = m_document.at(key);
			if (!value.is_number()) throw std::runtime_error("Non-numeric field");
			const float number = value.get<float>();
			if (!std::isfinite(number)) throw std::runtime_error("Non-finite field");
			return number;
		}

	  private:
		nlohmann::json m_document;
		std::unordered_map<const std::vector<std::string>*, std::array<float, 30>> m_cache;
	};
	struct Variant {
		std::string name;
		Bridge* bridge;
		bool directUtf8;
	};
	struct Case {
		std::string name;
		const Dataset* dataset;
		int reads; // 0=解析のみ、1=解析+一括取得、2=キャッシュ一括、3=キャッシュ個別
	};
	struct Task {
		std::string parser;
		std::function<double()> action;
		std::function<void()> collect;
		int iterations = 1;
		std::vector<double> times;
	};
	void Check(int result) {
		if (result < 0) throw std::runtime_error("Native bridge returned " + std::to_string(result));
	}
	std::string ReadFile(const fs::path& path) {
		std::ifstream input(path, std::ios::binary);
		if (!input) throw std::runtime_error("Cannot read " + path.string());
		return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	}
	std::string ProcessorBrand() {
		std::array<int, 12> brand{};
		for (int index = 0; index < 3; ++index)
			__cpuid(brand.data() + index * 4, 0x80000002 + index);
		const std::string text(reinterpret_cast<const char*>(brand.data()), 48);
		return text.substr(0, text.find('\0'));
	}
	double Batch(const std::function<double()>& action, int iterations) {
		const auto start = Clock::now();
		double sum = 0;
		for (int index = 0; index < iterations; ++index)
			sum += action();
		g_checksum = sum;
		return std::chrono::duration<double>(Clock::now() - start).count();
	}
	int Calibrate(const std::function<double()>& action) {
		int iterations = 1;
		while (true) {
			const double seconds = Batch(action, iterations);
			if (seconds >= 0.015 || iterations >= 1000000)
				return int(std::clamp(iterations * kSampleSeconds / std::max(seconds, 1e-9), 1.0, 2000000.0));
			iterations *= 2;
		}
	}
} // namespace

int wmain(int argc, wchar_t** argv) {
	try {
		if (argc < 3 || argc > 6)
			throw std::runtime_error("Usage: NativeBenchmark <directory> <run-id> [verify] [optimized|fair]");
		bool verifyOnly = false, includeOptimized = false, fairOptimized = false;
		for (int index = 3; index < argc; ++index) {
			if (std::wstring_view(argv[index]) == L"verify")
				verifyOnly = true;
			else if (std::wstring_view(argv[index]) == L"optimized")
				includeOptimized = true;
			else if (std::wstring_view(argv[index]) == L"fair")
				fairOptimized = true;
			else
				throw std::runtime_error("Unknown benchmark option");
		}
		const fs::path directory = fs::absolute(argv[1]);
		const int run = std::stoi(argv[2]);
		std::map<std::string, Dataset> datasets;
		std::ifstream manifest(directory / "datasets.tsv");
		std::string line;
		while (std::getline(manifest, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			const auto tab = line.find('\t');
			if (tab == std::string::npos) throw std::runtime_error("Invalid dataset manifest");
			auto& dataset = datasets[line.substr(0, tab)];
			dataset.name = line.substr(0, tab);
			dataset.documents.push_back(ReadFile(fs::u8path(line.substr(tab + 1))));
			dataset.bytes += dataset.documents.back().size();
		}
		if (datasets.empty()) throw std::runtime_error("Empty dataset manifest");
		std::vector<std::string> fields;
		std::ifstream fieldFile(directory / "fields.txt");
		while (std::getline(fieldFile, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			fields.push_back(line);
		}
		if (fields.size() != 30) throw std::runtime_error("Expected 30 generated AI fields");
		FieldIndices indices;
		indices.reserve(fields.size());
		for (size_t index = 0; index < fields.size(); ++index)
			indices.emplace(fields[index], index);
		std::unique_ptr<Bridge> redox, standard;
		std::vector<Variant> variants;
		if (fairOptimized) {
			redox = std::make_unique<Bridge>(directory / "MatchedRedox.dll", false, true);
			standard = std::make_unique<Bridge>(directory / "MatchedStandard.dll", false, true);
			variants = {{"REDox-matched", redox.get(), true}, {"STJ-matched", standard.get(), true},
				{"nlohmann-matched", nullptr, true}};
		} else {
			redox = std::make_unique<Bridge>(directory / "RedoxBenchmark.dll");
			standard = std::make_unique<Bridge>(directory / "StandardBenchmark.dll");
			variants = {{"REDox-current", redox.get(), false}, {"REDox-utf8", redox.get(), true},
				{"STJ-current", standard.get(), false}, {"STJ-utf8", standard.get(), true},
				{"nlohmann-json", nullptr, true}};
		}
		std::unique_ptr<Bridge> optimized;
		if (includeOptimized) {
			optimized = std::make_unique<Bridge>(directory / "GameOptimized.dll", true);
			variants.push_back({"REDox-game-optimized", optimized.get(), true});
		}
		std::vector<Case> cases;
		for (const auto& [name, dataset] : datasets)
			cases.push_back({"parse", &dataset, 0});
		const auto& aircraft = datasets.at("aircraft-ai");
		cases.push_back({"parse-and-read-30", &aircraft, 1});
		cases.push_back({"cached-read-30-batched", &aircraft, 2});
		cases.push_back({"cached-read-30-scalar", &aircraft, 3});
		if (fairOptimized) {
			// 正常値に加え、必要項目・型・有限性・出力不変・キャッシュ再利用も3者で確認する
			const auto original = nlohmann::json::parse(aircraft.documents[0]);
			std::vector<nlohmann::json> fixtures{original, nlohmann::json::array()};
			auto missing = original;
			missing.erase(fields[0]);
			fixtures.push_back(missing);
			for (auto invalid : {nlohmann::json(nullptr), nlohmann::json("1"), nlohmann::json(true),
					 nlohmann::json(3.5e38)}) {
				auto changed = original;
				changed[fields[0]] = invalid;
				fixtures.push_back(changed);
			}
			auto extra = original;
			extra["unused"] = "extra";
			fixtures.push_back(extra);
			for (const auto& fixture : fixtures) {
				const auto text = fixture.dump();
				MatchedNlohmannDocument document(text);
				std::array<float, 30> expected;
				expected.fill(-99);
				const int result = document.ReadAi(fields, indices, expected);
				for (Bridge* bridge : {redox.get(), standard.get()}) {
					intptr_t handle = 0;
					std::string temporary = text;
					Check(bridge->createUtf8(temporary.data(), int(temporary.size()), &handle));
					std::fill(temporary.begin(), temporary.end(), '\0');
					for (int repeat = 0; repeat < 2; ++repeat) {
						std::array<float, 30> actual;
						actual.fill(-99);
						if (bridge->readAi(handle, actual.data(), 30) != result || actual != expected)
							throw std::runtime_error("Matched validation/cache/ownership mismatch");
					}
					Check(bridge->release(handle));
				}
			}
		}
		std::ofstream csv(directory / ("native-" + std::to_string(run) + ".csv"));
		csv << "run,workload,dataset,documents,utf8_bytes,parser,sample,iterations,ns_per_operation\n";
		csv << std::fixed << std::setprecision(3);
		std::ofstream environment(directory / ("native-" + std::to_string(run) + "-environment.txt"));
		environment << "Compiler=_MSC_FULL_VER:" << _MSC_FULL_VER
					<< "\nMatchedAlgorithmsAndCache=" << fairOptimized
					<< "\nArchitecture=x64\nOptimization=/O2 /DNDEBUG\n"
					<< "Processor=" << ProcessorBrand() << '\n'
					<< "nlohmann=" << NLOHMANN_JSON_VERSION_MAJOR << '.' << NLOHMANN_JSON_VERSION_MINOR << '.'
					<< NLOHMANN_JSON_VERSION_PATCH << "\nSamples=" << kSamples
					<< "\nTargetSeconds=" << kSampleSeconds
					<< "\nIncludes=DOM construction/disposal, owned UTF8 input, native bridge where used\n"
					<< "Excludes=disk IO, DLL loading, cold start, DTO serialization, retained/peak memory "
					   "measurement\n";
		int caseIndex = 0;
		for (const auto& test : cases) {
			std::vector<Task> tasks;
			std::vector<std::pair<Bridge*, intptr_t>> cachedHandles;
			const auto cachedJson = nlohmann::json::parse(aircraft.documents[0]);
			std::unique_ptr<MatchedNlohmannDocument> matchedCached;
			if (fairOptimized && test.reads >= 2)
				matchedCached = std::make_unique<MatchedNlohmannDocument>(aircraft.documents[0]);
			for (const auto& variant : variants) {
				Bridge* bridge = variant.bridge;
				const auto create =
					bridge ? (variant.directUtf8 ? bridge->createUtf8 : bridge->create) : nullptr;
				intptr_t cached = 0;
				if (test.reads >= 2 && bridge) {
					Check(create(aircraft.documents[0].data(), int(aircraft.documents[0].size()), &cached));
					cachedHandles.emplace_back(bridge, cached);
				}
				const auto read = [&, bridge, cached](intptr_t handle) {
					double sum = 0;
					if (test.reads == 3) {
						for (const auto& field : fields) {
							float value = 0;
							Check(bridge->ReadSingle(handle, field.data(), int(field.size()), &value));
							sum += value;
						}
					} else {
						std::array<float, 30> values{};
						Check(bridge->readAi(handle, values.data(), int(values.size())));
						for (float value : values)
							sum += value;
					}
					return sum;
				};
				const auto action = [&, bridge, create, cached, read] {
					double sum = 0;
					if (!bridge) {
						if (fairOptimized) {
							const auto consume = [&](MatchedNlohmannDocument& document) {
								double total = 0;
								if (test.reads == 3) {
									for (const auto& field : fields)
										total += document.ReadNumber(field);
								} else {
									std::array<float, 30> values{};
									Check(document.ReadAi(fields, indices, values));
									for (float value : values)
										total += value;
								}
								return total;
							};
							if (test.reads >= 2)
								sum = consume(*matchedCached);
							else
								for (const auto& input : test.dataset->documents) {
									MatchedNlohmannDocument document(input);
									sum += test.reads == 0 ? 1 : consume(document);
								}
						} else if (test.reads >= 2) {
							for (const auto& field : fields)
								sum += cachedJson.at(field).get<float>();
						} else
							for (const auto& document : test.dataset->documents) {
								const auto parsed = nlohmann::json::parse(document);
								if (test.reads == 0)
									sum += 1;
								else
									for (const auto& field : fields)
										sum += parsed.at(field).get<float>();
							}
					} else if (test.reads >= 2)
						sum = read(cached);
					else
						for (const auto& document : test.dataset->documents) {
							intptr_t handle = 0;
							Check(create(document.data(), int(document.size()), &handle));
							sum += test.reads == 0 ? 1 : read(handle);
							Check(bridge->release(handle));
						}
					return sum;
				};
				tasks.push_back({variant.name, action, [bridge] {
									 if (bridge) bridge->collect();
								 }});
			}
			const double expected = tasks.back().action();
			for (auto& task : tasks) {
				if (task.action() != expected)
					throw std::runtime_error("Parser result mismatch: " + task.parser);
				if (verifyOnly) continue;
				const auto start = Clock::now();
				while (std::chrono::duration<double>(Clock::now() - start).count() < 0.15)
					Batch(task.action, 16);
				task.iterations = Calibrate(task.action);
			}
			if (verifyOnly) {
				for (const auto& [bridge, handle] : cachedHandles)
					Check(bridge->release(handle));
				std::cout << "Verified " << test.name << '/' << test.dataset->name << " across "
						  << tasks.size() << " paths" << std::endl;
				continue;
			}
			for (int sample = 0; sample < kSamples; ++sample) {
				for (int position = 0; position < int(tasks.size()); ++position) {
					auto& task = tasks[(position + sample + run + caseIndex) % tasks.size()];
					task.collect();
					const double ns = Batch(task.action, task.iterations) * 1e9 / task.iterations;
					task.times.push_back(ns);
					csv << run << ',' << test.name << ',' << test.dataset->name << ','
						<< test.dataset->documents.size() << ',' << test.dataset->bytes << ',' << task.parser
						<< ',' << sample << ',' << task.iterations << ',' << ns << '\n';
				}
			}
			csv.flush();
			std::cout << test.name << '/' << test.dataset->name;
			for (auto& task : tasks) {
				std::sort(task.times.begin(), task.times.end());
				std::cout << " | " << task.parser << ' ' << std::fixed << std::setprecision(3)
						  << task.times[kSamples / 2] / 1000 << " us";
			}
			std::cout << std::endl;
			for (const auto& [bridge, handle] : cachedHandles)
				Check(bridge->release(handle));
			++caseIndex;
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
