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
		using Collect = void(__cdecl*)();
		HMODULE module;
		Create create, createUtf8;
		Release release;
		ReadAi readAi;
		ReadNumber readNumber;
		Collect collect;
		explicit Bridge(const fs::path& path) {
			module = LoadLibraryW(path.c_str());
			if (!module) throw std::runtime_error("Native AOT DLL load failed: " + path.string());
			create = Load<Create>("Create");
			createUtf8 = Load<Create>("CreateUtf8");
			release = Load<Release>("Release");
			readAi = Load<ReadAi>("ReadAi");
			readNumber = Load<ReadNumber>("ReadNumber");
			collect = Load<Collect>("Collect");
		}
		template <typename T> T Load(const char* name) const {
			const auto address = GetProcAddress(module, name);
			if (!address) throw std::runtime_error(std::string("Missing export: ") + name);
			return reinterpret_cast<T>(address);
		}
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
		if (result != 0) throw std::runtime_error("Native bridge returned " + std::to_string(result));
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
		if (argc != 3 && argc != 4)
			throw std::runtime_error("Usage: NativeBenchmark <artifact-directory> <run-id> [verify]");
		const bool verifyOnly = argc == 4 && std::wstring_view(argv[3]) == L"verify";
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
		Bridge redox(directory / "RedoxBenchmark.dll"), standard(directory / "StandardBenchmark.dll");
		const std::array<Variant, 5> variants{{{"REDox-current", &redox, false}, {"REDox-utf8", &redox, true},
			{"STJ-current", &standard, false}, {"STJ-utf8", &standard, true},
			{"nlohmann-json", nullptr, true}}};
		std::vector<Case> cases;
		for (const auto& [name, dataset] : datasets)
			cases.push_back({"parse", &dataset, 0});
		const auto& aircraft = datasets.at("aircraft-ai");
		cases.push_back({"parse-and-read-30", &aircraft, 1});
		cases.push_back({"cached-read-30-batched", &aircraft, 2});
		cases.push_back({"cached-read-30-scalar", &aircraft, 3});
		std::ofstream csv(directory / ("native-" + std::to_string(run) + ".csv"));
		csv << "run,workload,dataset,documents,utf8_bytes,parser,sample,iterations,ns_per_operation\n";
		csv << std::fixed << std::setprecision(3);
		std::ofstream environment(directory / ("native-" + std::to_string(run) + "-environment.txt"));
		environment << "Compiler=_MSC_FULL_VER:" << _MSC_FULL_VER
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
							Check(bridge->readNumber(handle, field.data(), int(field.size()), &value));
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
						if (test.reads >= 2) {
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
				std::cout << "Verified " << test.name << '/' << test.dataset->name << " across five paths"
						  << std::endl;
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
