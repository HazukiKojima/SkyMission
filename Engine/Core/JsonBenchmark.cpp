#include "JsonBenchmark.h"
#include <Windows.h>
#include <winternl.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Engine {
	namespace {
		using GetStringFn = int(__cdecl*)(
			const unsigned char*, int,
			const unsigned char*, int,
			unsigned char*, int);
		using BatchGetStringFn = int(__cdecl*)(
			const unsigned char*, int,
			const unsigned char*, int,
			int, unsigned char*, int, double*);

		struct BenchmarkDataset {
			std::string name;
			std::filesystem::path path;
			std::string key;
			int depth;
			int propertyCount;
			std::string targetPosition;
			std::vector<unsigned char> json;
			std::vector<unsigned char> keyUtf8;
		};

		struct Measurement {
			double milliseconds;
			bool succeeded;
		};

		struct LibraryMeasurement {
			double nanoseconds;
			bool succeeded;
		};

		struct ModuleGuard {
			HMODULE handle;
			~ModuleGuard() {
				if (handle) {
					FreeLibrary(handle);
				}
			}
		};

		std::string PathToUtf8(const std::filesystem::path& path) {
			const std::wstring widePath = path.wstring();
			const int wideLength = static_cast<int>(widePath.size());
			const int byteLength = WideCharToMultiByte(
				CP_UTF8, 0, widePath.data(), wideLength, nullptr, 0, nullptr, nullptr);
			if (byteLength <= 0) {
				return {};
			}

			std::string utf8Path(byteLength, '\0');
			WideCharToMultiByte(
				CP_UTF8, 0, widePath.data(), wideLength,
				utf8Path.data(), byteLength, nullptr, nullptr);
			return utf8Path;
		}

		std::vector<unsigned char> ReadBytes(const std::filesystem::path& path) {
			std::ifstream file(path, std::ios::binary);
			if (!file) {
				throw std::runtime_error("Failed to open benchmark JSON: " + PathToUtf8(path));
			}

			file.seekg(0, std::ios::end);
			const std::streamoff length = file.tellg();
			if (length <= 0 || length > (std::numeric_limits<int>::max)()) {
				throw std::runtime_error("Benchmark JSON has an invalid size: " + PathToUtf8(path));
			}

			std::vector<unsigned char> bytes(static_cast<size_t>(length));
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char*>(bytes.data()), length);
			if (!file) {
				throw std::runtime_error("Failed to read benchmark JSON: " + PathToUtf8(path));
			}
			return bytes;
		}

		bool ReadResult(
			GetStringFn reader,
			const BenchmarkDataset& dataset,
			std::array<unsigned char, 4096>& output,
			int& result) {
			result = reader(
				dataset.json.data(), static_cast<int>(dataset.json.size()),
				dataset.keyUtf8.data(), static_cast<int>(dataset.keyUtf8.size()),
				output.data(), static_cast<int>(output.size()));
			return result >= 0;
		}

		bool ResultsMatch(
			GetStringFn redox,
			GetStringFn systemTextJson,
			const BenchmarkDataset& dataset) {
			std::array<unsigned char, 4096> redoxOutput{};
			std::array<unsigned char, 4096> systemOutput{};
			int redoxLength = 0;
			int systemLength = 0;
			return ReadResult(redox, dataset, redoxOutput, redoxLength) &&
				ReadResult(systemTextJson, dataset, systemOutput, systemLength) &&
				redoxLength == systemLength &&
				std::equal(redoxOutput.begin(), redoxOutput.begin() + redoxLength, systemOutput.begin());
		}

		bool BatchResultsMatch(
			BatchGetStringFn redox,
			BatchGetStringFn systemTextJson,
			const BenchmarkDataset& dataset,
			int iterations) {
			std::array<unsigned char, 4096> redoxOutput{};
			std::array<unsigned char, 4096> systemOutput{};
			double redoxNanoseconds = 0.0;
			double systemNanoseconds = 0.0;
			const int redoxLength = redox(
				dataset.json.data(), static_cast<int>(dataset.json.size()),
				dataset.keyUtf8.data(), static_cast<int>(dataset.keyUtf8.size()),
				iterations, redoxOutput.data(), static_cast<int>(redoxOutput.size()), &redoxNanoseconds);
			const int systemLength = systemTextJson(
				dataset.json.data(), static_cast<int>(dataset.json.size()),
				dataset.keyUtf8.data(), static_cast<int>(dataset.keyUtf8.size()),
				iterations, systemOutput.data(), static_cast<int>(systemOutput.size()), &systemNanoseconds);
			return redoxLength >= 0 && redoxLength == systemLength &&
				std::equal(redoxOutput.begin(), redoxOutput.begin() + redoxLength, systemOutput.begin());
		}

		Measurement Measure(
			GetStringFn reader,
			const BenchmarkDataset& dataset,
			int expectedLength,
			int iterations) {
			std::array<unsigned char, 4096> output{};
			const auto start = std::chrono::steady_clock::now();
			bool succeeded = true;
			for (int i = 0; i < iterations; ++i) {
				const int result = reader(
					dataset.json.data(), static_cast<int>(dataset.json.size()),
					dataset.keyUtf8.data(), static_cast<int>(dataset.keyUtf8.size()),
					output.data(), static_cast<int>(output.size()));
				if (result != expectedLength) {
					succeeded = false;
					break;
				}
			}
			const auto elapsed = std::chrono::steady_clock::now() - start;
			return {
				std::chrono::duration<double, std::milli>(elapsed).count(),
				succeeded
			};
		}

		LibraryMeasurement MeasureLibrary(
			BatchGetStringFn reader,
			const BenchmarkDataset& dataset,
			int expectedLength,
			int iterations) {
			std::array<unsigned char, 4096> output{};
			double elapsedNanoseconds = 0.0;
			const int result = reader(
				dataset.json.data(), static_cast<int>(dataset.json.size()),
				dataset.keyUtf8.data(), static_cast<int>(dataset.keyUtf8.size()),
				iterations, output.data(), static_cast<int>(output.size()), &elapsedNanoseconds);
			return { elapsedNanoseconds, result == expectedLength && elapsedNanoseconds > 0.0 };
		}

		double Median(std::vector<double> values) {
			std::sort(values.begin(), values.end());
			const size_t middle = values.size() / 2;
			if (values.size() % 2 == 0) {
				return (values[middle - 1] + values[middle]) / 2.0;
			}
			return values[middle];
		}

		std::string CsvEscape(const std::string& value) {
			std::string escaped = "\"";
			for (char character : value) {
				if (character == '\"') {
					escaped += "\"\"";
				}
				else {
					escaped += character;
				}
			}
			escaped += "\"";
			return escaped;
		}

		std::filesystem::path ExecutableDirectory() {
			std::wstring executablePath(32768, L'\0');
			const DWORD length = GetModuleFileNameW(
				nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
			if (length == 0 || length >= executablePath.size()) {
				throw std::runtime_error("Failed to get executable path.");
			}
			executablePath.resize(length);
			return std::filesystem::path(executablePath).parent_path();
		}

		std::wstring ReadEnvironmentVariableW(const wchar_t* name) {
			const DWORD requiredLength = GetEnvironmentVariableW(name, nullptr, 0);
			if (requiredLength == 0) {
				return {};
			}
			std::wstring value(requiredLength, L'\0');
			const DWORD length = GetEnvironmentVariableW(name, value.data(), requiredLength);
			if (length == 0 || length >= requiredLength) {
				return {};
			}
			value.resize(length);
			return value;
		}

		std::string ReadEnvironmentVariableA(const char* name);

		int ReadPositiveInt(const char* name, int defaultValue) {
			const std::string value = ReadEnvironmentVariableA(name);
			if (value.empty()) {
				return defaultValue;
			}

			int parsedValue = 0;
			const auto [end, error] = std::from_chars(
				value.data(), value.data() + value.size(), parsedValue);
			return error == std::errc{} && end == value.data() + value.size() && parsedValue > 0
				? parsedValue
				: defaultValue;
		}

		std::string ReadEnvironmentVariableA(const char* name) {
			const DWORD requiredLength = GetEnvironmentVariableA(name, nullptr, 0);
			if (requiredLength == 0) {
				return {};
			}
			std::string value(requiredLength, '\0');
			const DWORD length = GetEnvironmentVariableA(name, value.data(), requiredLength);
			if (length == 0 || length >= requiredLength) {
				return {};
			}
			value.resize(length);
			return value;
		}

		std::string ReadProcessorName() {
			HKEY key = nullptr;
			if (RegOpenKeyExW(
				HKEY_LOCAL_MACHINE,
				L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
				0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
				return "unknown";
			}

			wchar_t processorName[256]{};
			DWORD byteLength = sizeof(processorName);
			const LONG status = RegQueryValueExW(
				key, L"ProcessorNameString", nullptr, nullptr,
				reinterpret_cast<LPBYTE>(processorName), &byteLength);
			RegCloseKey(key);
			return status == ERROR_SUCCESS ? PathToUtf8(processorName) : "unknown";
		}

		std::string ReadWindowsVersion() {
			const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
			if (!ntdll) {
				return "unknown";
			}
			using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
			auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
				GetProcAddress(ntdll, "RtlGetVersion"));
			if (!rtlGetVersion) {
				return "unknown";
			}

			RTL_OSVERSIONINFOW version{};
			version.dwOSVersionInfoSize = sizeof(version);
			if (rtlGetVersion(&version) != 0) {
				return "unknown";
			}
			return "Windows " + std::to_string(version.dwMajorVersion) + "." +
				std::to_string(version.dwMinorVersion) + " (build " +
				std::to_string(version.dwBuildNumber) + ")";
		}

		void WriteEnvironmentMetadata(
			const std::filesystem::path& executableDirectory,
			int warmupCount,
			int iterationCount,
			int roundCount) {
			const auto metadataPath = executableDirectory / L"JsonBenchmarkEnvironment.txt";
			std::ofstream metadata(metadataPath, std::ios::binary);
			if (!metadata) {
				throw std::runtime_error("Failed to create benchmark environment metadata.");
			}

			SYSTEM_INFO systemInfo{};
			GetNativeSystemInfo(&systemInfo);
			MEMORYSTATUSEX memoryStatus{};
			memoryStatus.dwLength = sizeof(memoryStatus);
			const unsigned long long totalMemory = GlobalMemoryStatusEx(&memoryStatus)
				? memoryStatus.ullTotalPhys
				: 0;

			std::ifstream sdkVersionFile(executableDirectory / L"DotNetSdkVersion.txt", std::ios::binary);
			std::string sdkVersion;
			std::getline(sdkVersionFile, sdkVersion);
			if (sdkVersion.empty()) {
				sdkVersion = "not recorded";
			}

#ifdef NDEBUG
			constexpr const char* buildConfiguration = "Release";
#else
			constexpr const char* buildConfiguration = "Debug";
#endif
			metadata << "OS=" << ReadWindowsVersion() << '\n'
				<< "CPU=" << ReadProcessorName() << '\n'
				<< "LogicalProcessors=" << systemInfo.dwNumberOfProcessors << '\n'
				<< "TotalPhysicalMemoryBytes=" << totalMemory << '\n'
				<< "CxxBuildConfiguration=" << buildConfiguration << '\n'
				<< "WarmupIterations=" << warmupCount << '\n'
				<< "IterationsPerRound=" << iterationCount << '\n'
				<< "MeasurementRounds=" << roundCount << '\n'
				<< ".NETTargetFramework=net10.0\n"
				<< ".NETSDK=" << sdkVersion << '\n'
				<< "REDoxNuGetPackage=CAPCOM.REDox 1.0.3\n";
		}
	}

	int RunJsonBenchmark() {
		const int warmupCount = ReadPositiveInt("REDOX_BENCH_WARMUP", 20);
		const int iterationCount = ReadPositiveInt("REDOX_BENCH_ITERATIONS", 1000);
		const int roundCount = ReadPositiveInt("REDOX_BENCH_ROUNDS", 5);

		const auto executableDirectory = ExecutableDirectory();
		WriteEnvironmentMetadata(executableDirectory, warmupCount, iterationCount, roundCount);
		const auto benchmarkDirectory = executableDirectory / L"..\\..\\Assets\\Benchmark";
		const auto dllPath = executableDirectory / L"REDoxWrapper.dll";
		ModuleGuard module{ LoadLibraryW(dllPath.c_str()) };
		if (!module.handle) {
			throw std::runtime_error("Failed to load REDoxWrapper.dll.");
		}

		auto redox = reinterpret_cast<GetStringFn>(
			GetProcAddress(module.handle, "MyRedoxGetString"));
		auto systemTextJson = reinterpret_cast<GetStringFn>(
			GetProcAddress(module.handle, "MySystemTextJsonGetString"));
		auto redoxBatch = reinterpret_cast<BatchGetStringFn>(
			GetProcAddress(module.handle, "MyRedoxBenchmarkBatch"));
		auto systemTextJsonBatch = reinterpret_cast<BatchGetStringFn>(
			GetProcAddress(module.handle, "MySystemTextJsonBenchmarkBatch"));
		if (!redox || !systemTextJson || !redoxBatch || !systemTextJsonBatch) {
			throw std::runtime_error("Failed to find all JSON benchmark exports.");
		}

		std::vector<BenchmarkDataset> datasets;
		for (const auto& [name, fileName] : std::array{
			std::pair{ "Small", L"Small.json" },
			std::pair{ "Medium", L"Medium.json" },
			std::pair{ "Large", L"Large.json" } }) {
			BenchmarkDataset dataset{
				name,
				benchmarkDirectory / fileName,
				"target",
				1,
				2,
				"last"
			};
			dataset.json = ReadBytes(dataset.path);
			dataset.keyUtf8.assign(dataset.key.begin(), dataset.key.end());
			datasets.push_back(std::move(dataset));
		}

		const std::wstring officialPath = ReadEnvironmentVariableW(L"REDOX_OFFICIAL_JSON");
		if (!officialPath.empty()) {
			const std::string officialKey = ReadEnvironmentVariableA("REDOX_OFFICIAL_KEY");
			BenchmarkDataset dataset{
				"Official",
				std::filesystem::path(officialPath),
				officialKey.empty() ? "name" : officialKey,
				0,
				0,
				"not-recorded"
			};
			if (dataset.path.is_relative()) {
				dataset.path = std::filesystem::absolute(dataset.path);
			}
			dataset.json = ReadBytes(dataset.path);
			dataset.keyUtf8.assign(dataset.key.begin(), dataset.key.end());
			datasets.push_back(std::move(dataset));
		}

		const auto csvPath = executableDirectory / L"JsonBenchmarkResults.csv";
		std::ofstream csv(csvPath, std::ios::binary);
		if (!csv) {
			throw std::runtime_error("Failed to create benchmark CSV output.");
		}
		csv << "Type,Dataset,JSONFile,JSONBytes,Depth,PropertyCount,TargetPosition,Key,Library,Round,Iterations,ElapsedMs,AverageUs,MedianUs,Speedup,ReductionPercent,Result\n";

		OutputDebugStringA("JSON benchmark started. Integration timings include the C++/NativeAOT call and wrapper processing; library timings cover parsing and property lookup only.\n");
		for (const auto& dataset : datasets) {
			if (!ResultsMatch(redox, systemTextJson, dataset)) {
				throw std::runtime_error("JSON reader results differ for dataset: " + dataset.name);
			}
			if (!BatchResultsMatch(redoxBatch, systemTextJsonBatch, dataset, 1)) {
				throw std::runtime_error("JSON batch reader results differ for dataset: " + dataset.name);
			}

			std::array<unsigned char, 4096> output{};
			int expectedLength = 0;
			if (!ReadResult(redox, dataset, output, expectedLength)) {
				throw std::runtime_error("JSON reader failed for dataset: " + dataset.name);
			}

			for (int i = 0; i < warmupCount; ++i) {
				std::array<unsigned char, 4096> warmupOutput{};
				int firstLength = 0;
				int secondLength = 0;
				const bool redoxFirst = i % 2 == 0;
				const bool firstSucceeded = ReadResult(
					redoxFirst ? redox : systemTextJson, dataset, warmupOutput, firstLength);
				const bool secondSucceeded = ReadResult(
					redoxFirst ? systemTextJson : redox, dataset, warmupOutput, secondLength);
				const LibraryMeasurement firstBatch = MeasureLibrary(
					redoxFirst ? redoxBatch : systemTextJsonBatch, dataset, expectedLength, 1);
				const LibraryMeasurement secondBatch = MeasureLibrary(
					redoxFirst ? systemTextJsonBatch : redoxBatch, dataset, expectedLength, 1);
				if (!firstSucceeded || !secondSucceeded ||
					firstLength != expectedLength || secondLength != expectedLength ||
					!firstBatch.succeeded || !secondBatch.succeeded) {
					throw std::runtime_error("JSON warmup failed for dataset: " + dataset.name);
				}
			}

			std::vector<double> integrationRedoxRoundUs;
			std::vector<double> integrationSystemRoundUs;
			std::vector<double> libraryRedoxRoundUs;
			std::vector<double> librarySystemRoundUs;
			for (int round = 0; round < roundCount; ++round) {
				const bool redoxFirst = round % 2 == 0;
				const Measurement integrationFirst = Measure(
					redoxFirst ? redox : systemTextJson,
					dataset, expectedLength, iterationCount);
				const Measurement integrationSecond = Measure(
					redoxFirst ? systemTextJson : redox,
					dataset, expectedLength, iterationCount);
				const Measurement integrationRedox = redoxFirst ? integrationFirst : integrationSecond;
				const Measurement integrationSystem = redoxFirst ? integrationSecond : integrationFirst;

				const LibraryMeasurement libraryFirst = MeasureLibrary(
					redoxFirst ? redoxBatch : systemTextJsonBatch, dataset, expectedLength, iterationCount);
				const LibraryMeasurement librarySecond = MeasureLibrary(
					redoxFirst ? systemTextJsonBatch : redoxBatch, dataset, expectedLength, iterationCount);
				const LibraryMeasurement libraryRedox = redoxFirst ? libraryFirst : librarySecond;
				const LibraryMeasurement librarySystem = redoxFirst ? librarySecond : libraryFirst;

				if (!integrationRedox.succeeded || !integrationSystem.succeeded ||
					!libraryRedox.succeeded || !librarySystem.succeeded ||
					!ResultsMatch(redox, systemTextJson, dataset) ||
					!BatchResultsMatch(redoxBatch, systemTextJsonBatch, dataset, 1)) {
					throw std::runtime_error("JSON benchmark result validation failed for dataset: " + dataset.name);
				}

				const double integrationRedoxUs = integrationRedox.milliseconds * 1000.0 / iterationCount;
				const double integrationSystemUs = integrationSystem.milliseconds * 1000.0 / iterationCount;
				const double libraryRedoxUs = libraryRedox.nanoseconds / 1000.0 / iterationCount;
				const double librarySystemUs = librarySystem.nanoseconds / 1000.0 / iterationCount;
				integrationRedoxRoundUs.push_back(integrationRedoxUs);
				integrationSystemRoundUs.push_back(integrationSystemUs);
				libraryRedoxRoundUs.push_back(libraryRedoxUs);
				librarySystemRoundUs.push_back(librarySystemUs);
				csv << "IntegrationRound," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",RE:Dox," << round + 1 << ',' << iterationCount << ','
					<< integrationRedox.milliseconds << ',' << integrationRedoxUs << ",,,,Success\n";
				csv << "IntegrationRound," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",System.Text.Json," << round + 1 << ',' << iterationCount << ','
					<< integrationSystem.milliseconds << ',' << integrationSystemUs << ",,,,Success\n";
				csv << "LibraryRound," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",RE:Dox," << round + 1 << ',' << iterationCount << ','
					<< libraryRedox.nanoseconds / 1000000.0 << ','
					<< libraryRedoxUs << ",,,,Success\n";
				csv << "LibraryRound," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",System.Text.Json," << round + 1 << ',' << iterationCount << ','
					<< librarySystem.nanoseconds / 1000000.0 << ','
					<< librarySystemUs << ",,,,Success\n";
			}

			auto writeSummary = [&](const char* type, const std::vector<double>& redoxValues,
				const std::vector<double>& systemValues) {
				const double redoxMedianUs = Median(redoxValues);
				const double systemMedianUs = Median(systemValues);
				if (redoxMedianUs <= 0.0 || systemMedianUs <= 0.0) {
					throw std::runtime_error("Invalid benchmark median for dataset: " + dataset.name);
				}
				const double speedup = systemMedianUs / redoxMedianUs;
				const double reductionPercent = (1.0 - redoxMedianUs / systemMedianUs) * 100.0;
				csv << type << "Summary," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",RE:Dox,,,,," << redoxMedianUs << ",,,Success\n";
				csv << type << "Summary," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",System.Text.Json,,,,," << systemMedianUs << ",,,Success\n";
				csv << type << "Comparison," << CsvEscape(dataset.name) << ','
					<< CsvEscape(PathToUtf8(dataset.path.filename())) << ',' << dataset.json.size() << ','
					<< dataset.depth << ',' << dataset.propertyCount << ',' << CsvEscape(dataset.targetPosition) << ','
					<< CsvEscape(dataset.key) << ",RE:Dox vs System.Text.Json,,,,,," << speedup << ','
					<< reductionPercent << ",Success\n";

				std::string report = std::string(type) + " " + dataset.name + ": RE:Dox median " +
					std::to_string(redoxMedianUs) + " us, System.Text.Json median " +
					std::to_string(systemMedianUs) + " us, speedup " +
					std::to_string(speedup) + "x, time reduction " +
					std::to_string(reductionPercent) + "%\n";
				OutputDebugStringA(report.c_str());
			};
			writeSummary("Integration", integrationRedoxRoundUs, integrationSystemRoundUs);
			writeSummary("Library", libraryRedoxRoundUs, librarySystemRoundUs);
		}

		OutputDebugStringW((L"JSON benchmark CSV: " + csvPath.wstring() + L"\n").c_str());
		return 0;
	}
}
