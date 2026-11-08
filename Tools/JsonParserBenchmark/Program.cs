using System.Diagnostics;
using System.Globalization;
using System.Runtime;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using REDoxWrapper.Generated;
using REDox.Json;
using RedoxDocument = REDox.Json.JsonDocument;
using StandardDocument = System.Text.Json.JsonDocument;

namespace JsonParserBenchmark;

internal sealed record Input(string Name, string Text, byte[] Utf8);
internal sealed record Dataset(string Name, Input[] Files)
{
    public int Bytes => Files.Sum(file => file.Utf8.Length);
}
internal sealed record Pair(string Workload, Dataset Dataset, Func<double> Redox, Func<double> Standard);
internal readonly record struct Sample(int Iterations, double Nanoseconds, double ManagedBytes);

internal static class Program
{
    private static double s_checksum;
    private const int Samples = 9;
    private const double SampleMilliseconds = 100;

    private static int Main(string[] args)
    {
        if (args.Length != 3)
        {
            Console.Error.WriteLine("Usage: JsonParserBenchmark <repository> <output-directory> <run-id>");
            return 2;
        }
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        string repository = Path.GetFullPath(args[0]);
        string output = Path.GetFullPath(args[1]);
        Directory.CreateDirectory(output);
        string mode = RuntimeFeature.IsDynamicCodeSupported ? "jit" : "aot";
        string run = args[2];
        WriteEnvironment(Path.Combine(output, $"{mode}-{run}-environment.txt"));
        Dataset[] datasets = BuildDatasets(repository);
        WriteDatasets(datasets, output);
        if (run == "prepare") return 0;
        List<Pair> pairs = [];
        foreach (Dataset dataset in datasets)
        {
            pairs.Add(new("parse-string", dataset,
                () => ParseRedox(dataset, false), () => ParseStandard(dataset, false)));
            // 補足の純.NET比較では、両者とも公開UTF-8 APIを直接呼ぶ
            pairs.Add(new("parse-utf8", dataset,
                () => ParseRedox(dataset, true), () => ParseStandard(dataset, true)));
        }
        Dataset aircraft = datasets[0];
        Input aircraftInput = aircraft.Files[0];
        pairs.Add(new("parse-utf8-and-read-30-fields", aircraft,
            () => ParseAndReadRedox(aircraftInput), () => ParseAndReadStandard(aircraftInput)));
        using RedoxDocument cachedRedox = RedoxDocument.Parse(aircraftInput.Text);
        using StandardDocument cachedStandard = StandardDocument.Parse(aircraftInput.Utf8);
        pairs.Add(new("cached-read-30-fields", aircraft,
            () => ReadRedox(cachedRedox), () => ReadStandard(cachedStandard)));

        using StreamWriter csv = new(Path.Combine(output, $"{mode}-{run}.csv"), false, new UTF8Encoding(false));
        csv.WriteLine("mode,run,workload,dataset,documents,utf8_bytes,parser,sample,iterations,ns_per_operation,managed_bytes_per_operation");
        int pairIndex = 0;
        foreach (Pair pair in pairs)
        {
            double redoxValue = pair.Redox();
            double standardValue = pair.Standard();
            if (!double.IsFinite(redoxValue) || redoxValue != standardValue)
                throw new InvalidOperationException($"Different results: {pair.Workload}/{pair.Dataset.Name}: {redoxValue} != {standardValue}");
            Warm(pair.Redox);
            Warm(pair.Standard);
            int redoxIterations = Calibrate(pair.Redox);
            int standardIterations = Calibrate(pair.Standard);
            List<double> redoxTimes = [], standardTimes = [];
            for (int sample = 0; sample < Samples; ++sample)
            {
                // 同じプロセス内で先行順を交互にし、常に片方が先に走る偏りを避ける
                bool redoxFirst = (sample + pairIndex + int.Parse(run)) % 2 == 0;
                if (redoxFirst)
                {
                    Record("REDox", pair.Redox, redoxIterations, redoxTimes);
                    Record("System.Text.Json", pair.Standard, standardIterations, standardTimes);
                }
                else
                {
                    Record("System.Text.Json", pair.Standard, standardIterations, standardTimes);
                    Record("REDox", pair.Redox, redoxIterations, redoxTimes);
                }
                void Record(string parser, Func<double> action, int iterations, List<double> times)
                {
                    Sample measured = Measure(action, iterations);
                    times.Add(measured.Nanoseconds);
                    csv.WriteLine($"{mode},{run},{pair.Workload},{pair.Dataset.Name},{pair.Dataset.Files.Length},{pair.Dataset.Bytes},{parser},{sample},{measured.Iterations},{measured.Nanoseconds:F3},{measured.ManagedBytes:F3}");
                }
            }
            csv.Flush();
            redoxTimes.Sort();
            standardTimes.Sort();
            Console.WriteLine($"{mode}/{run} {pair.Workload,-30} {pair.Dataset.Name,-23} REDox {redoxTimes[Samples / 2] / 1000,10:F3} us; STJ {standardTimes[Samples / 2] / 1000,10:F3} us");
            ++pairIndex;
        }
        GC.KeepAlive(s_checksum);
        return 0;
    }

    private static Dataset[] BuildDatasets(string repository)
    {
        Input Load(string path)
        {
            // BOMの有無を正規化する。両者に同一内容を与え、ディスクI/Oは測定に含めない
            string text = File.ReadAllText(path);
            return new(Path.GetFileName(path), text, Encoding.UTF8.GetBytes(text));
        }
        Dataset Numeric(string name, int count)
        {
            using MemoryStream stream = new();
            using (System.Text.Json.Utf8JsonWriter writer = new(stream))
            {
                writer.WriteStartArray();
                for (int index = 0; index < count; ++index) writer.WriteNumberValue(index * 0.125 - 500);
                writer.WriteEndArray();
            }
            byte[] utf8 = stream.ToArray();
            return new(name, [new(name, Encoding.UTF8.GetString(utf8), utf8)]);
        }
        using MemoryStream mixed = new();
        using (System.Text.Json.Utf8JsonWriter writer = new(mixed,
            new() { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping }))
        {
            writer.WriteStartArray();
            for (int index = 0; index < 1000; ++index)
            {
                writer.WriteStartObject();
                writer.WriteNumber("id", index);
                writer.WriteString("name", $"敵編隊-{index:D4}");
                writer.WriteStartArray("position");
                writer.WriteNumberValue(index * 1.25);
                writer.WriteNumberValue(1500.5);
                writer.WriteNumberValue(-5000.25 - index);
                writer.WriteEndArray();
                writer.WriteNumber("health", 120);
                writer.WriteBoolean("active", index % 3 != 0);
                writer.WriteStartArray("tags");
                writer.WriteStringValue("fighter");
                writer.WriteStringValue("patrol");
                writer.WriteEndArray();
                writer.WriteEndObject();
            }
            writer.WriteEndArray();
        }
        byte[] mixedBytes = mixed.ToArray();
        return [
            new("aircraft-ai", [Load(Path.Combine(repository, "Assets/Aircraft/F4U4Corsair/aiParameters.json"))]),
            new("all-game-configs", Directory.GetFiles(Path.Combine(repository, "Assets"), "*.json", SearchOption.AllDirectories)
                .OrderBy(path => path, StringComparer.Ordinal).Select(Load).ToArray()),
            Numeric("numeric-10000", 10000),
            Numeric("numeric-100000", 100000),
            new("aircraft-array-1000", [new("aircraft-array-1000", Encoding.UTF8.GetString(mixedBytes), mixedBytes)])];
    }

    private static void WriteDatasets(Dataset[] datasets, string output)
    {
        using StreamWriter manifest = new(Path.Combine(output, "datasets.tsv"), false, new UTF8Encoding(false));
        foreach (Dataset dataset in datasets)
        {
            string directory = Path.Combine(output, "payloads", dataset.Name);
            Directory.CreateDirectory(directory);
            for (int index = 0; index < dataset.Files.Length; ++index)
            {
                string path = Path.Combine(directory, $"{index:D3}.json");
                File.WriteAllBytes(path, dataset.Files[index].Utf8);
                manifest.WriteLine($"{dataset.Name}\t{path}");
            }
        }
        File.WriteAllLines(Path.Combine(output, "fields.txt"), GeneratedAircraftAIParameters.FieldNames);
    }

    private static double ParseRedox(Dataset dataset, bool utf8)
    {
        double result = 0;
        foreach (Input input in dataset.Files)
        {
            using RedoxDocument document = utf8 ? RedoxDocument.Parse(input.Utf8) : RedoxDocument.Parse(input.Text);
            result += (int)document.RootElement.ValueKind;
        }
        return result;
    }

    private static double ParseStandard(Dataset dataset, bool utf8)
    {
        double result = 0;
        foreach (Input input in dataset.Files)
        {
            using StandardDocument document = utf8 ? StandardDocument.Parse(input.Utf8) : StandardDocument.Parse(input.Text);
            result += (int)document.RootElement.ValueKind;
        }
        return result;
    }

    private static double ParseAndReadRedox(Input input)
    {
        using RedoxDocument document = RedoxDocument.Parse(input.Utf8);
        return ReadRedox(document);
    }

    private static double ParseAndReadStandard(Input input)
    {
        using StandardDocument document = StandardDocument.Parse(input.Utf8);
        return ReadStandard(document);
    }

    private static double ReadRedox(RedoxDocument document)
    {
        double sum = 0;
        foreach (string field in GeneratedAircraftAIParameters.FieldNames)
        {
            if (!document.RootElement.TryGetProperty(field, out var value) || !value.TryGetSingle(out float number))
                throw new InvalidOperationException($"REDox failed to read {field}");
            sum += number;
        }
        return sum;
    }

    private static double ReadStandard(StandardDocument document)
    {
        double sum = 0;
        foreach (string field in GeneratedAircraftAIParameters.FieldNames)
        {
            if (!document.RootElement.TryGetProperty(field, out var value) || !value.TryGetSingle(out float number))
                throw new InvalidOperationException($"STJ failed to read {field}");
            sum += number;
        }
        return sum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void RunBatch(Func<double> action, int iterations)
    {
        double sum = 0;
        for (int index = 0; index < iterations; ++index) sum += action();
        Volatile.Write(ref s_checksum, sum);
    }

    private static void Warm(Func<double> action)
    {
        Stopwatch timer = Stopwatch.StartNew();
        while (timer.ElapsedMilliseconds < 200) RunBatch(action, 16);
    }

    private static int Calibrate(Func<double> action)
    {
        int iterations = 1;
        while (true)
        {
            long start = Stopwatch.GetTimestamp();
            RunBatch(action, iterations);
            double elapsed = Stopwatch.GetElapsedTime(start).TotalMilliseconds;
            if (elapsed >= 15 || iterations >= 1_000_000)
                return (int)Math.Clamp(iterations * SampleMilliseconds / Math.Max(elapsed, 0.001), 1, 2_000_000);
            iterations *= 2;
        }
    }

    private static Sample Measure(Func<double> action, int iterations)
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        long allocated = GC.GetAllocatedBytesForCurrentThread();
        long start = Stopwatch.GetTimestamp();
        RunBatch(action, iterations);
        double elapsedNs = Stopwatch.GetElapsedTime(start).TotalNanoseconds;
        long allocatedBytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
        return new(iterations, elapsedNs / iterations, (double)allocatedBytes / iterations);
    }

    private static void WriteEnvironment(string path)
    {
        File.WriteAllLines(path, [
            $"UTC={DateTimeOffset.UtcNow:O}", $"Framework={RuntimeInformation.FrameworkDescription}",
            $"OS={RuntimeInformation.OSDescription}", $"Architecture={RuntimeInformation.ProcessArchitecture}",
            $"Processor={Environment.GetEnvironmentVariable("PROCESSOR_IDENTIFIER")}",
            $"LogicalProcessors={Environment.ProcessorCount}", $"ServerGC={GCSettings.IsServerGC}",
            $"DynamicCode={RuntimeFeature.IsDynamicCodeSupported}",
            $"REDox={typeof(RedoxDocument).Assembly.GetName().Version}",
            $"System.Text.Json={typeof(StandardDocument).Assembly.GetName().Version}",
            $"TieredCompilation={Environment.GetEnvironmentVariable("DOTNET_TieredCompilation")}",
            $"SamplesPerProcess={Samples}", $"TargetMillisecondsPerSample={SampleMilliseconds}",
            "Allocation=managed bytes on current thread only; excludes native allocations and pooled capacity",
            "Includes=DOM construction/disposal; UTF8 conversion only where labelled; 30-field float extraction where labelled",
            "Excludes=disk IO, DLL loading, PInvoke/interop, wrapper handle dictionary/lock, startup compilation, DTO deserialization"
        ]);
    }
}
