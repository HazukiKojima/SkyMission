using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using REDoxWrapper.Generated;
#if STANDARD_JSON
using System.Text.Json;
#else
using REDox.Json;
#endif

namespace JsonParserBenchmark;

// 両.NETライブラリでこのソースを共有し、一括走査・検証・キャッシュ・同期をそろえる
public static unsafe class MatchedBridge
{
    private static readonly object Gate = new();
    private static readonly Dictionary<nint, State> Documents = [];
    private static readonly Dictionary<string, int> Indices = GeneratedAircraftAIParameters.FieldNames
        .Select((name, index) => (name, index)).ToDictionary(item => item.name, item => item.index, StringComparer.Ordinal);
    private static long s_nextHandle;

    private sealed class State(JsonDocument document) : IDisposable
    {
        internal JsonDocument Document { get; } = document;
        private readonly Dictionary<string[], float[]> _cache = [];

        internal int Read(Span<float> output)
        {
            string[] fields = GeneratedAircraftAIParameters.FieldNames;
            if (output.Length != fields.Length) return -1;
            if (!_cache.TryGetValue(fields, out float[]? values))
            {
                var root = Document.RootElement;
                if (root.ValueKind != JsonValueKind.Object) return -3;
                values = new float[fields.Length];
                Span<byte> seen = stackalloc byte[GeneratedAircraftAIParameters.FieldCount];
                seen.Clear();
                int found = 0;
                foreach (var property in root.EnumerateObject())
                {
                    if (!Indices.TryGetValue(property.Name, out int index) || seen[index] != 0) continue;
                    var value = property.Value;
                    if (value.ValueKind != JsonValueKind.Number ||
                        !value.TryGetSingle(out float number) || !float.IsFinite(number)) return -5;
                    values[index] = number;
                    seen[index] = 1;
                    if (++found == fields.Length) break;
                }
                if (found != fields.Length) return -5;
                _cache.Add(fields, values);
            }
            values.AsSpan().CopyTo(output);
            return 0;
        }

        public void Dispose()
        {
            _cache.Clear();
            Document.Dispose();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "CreateUtf8", CallConvs = [typeof(CallConvCdecl)])]
    public static int CreateUtf8(byte* input, int length, nint* output)
    {
        if (input == null || length <= 0 || output == null) return -1;
        *output = 0;
        JsonDocument document;
        try
        {
#if STANDARD_JSON
            document = JsonDocument.Parse(new ReadOnlySpan<byte>(input, length).ToArray());
#else
            document = JsonDocument.Parse(new ReadOnlySpan<byte>(input, length));
#endif
        }
        catch { return -2; }
        try
        {
            lock (Gate)
            {
                nint handle = (nint)Interlocked.Increment(ref s_nextHandle);
                Documents.Add(handle, new State(document));
                *output = handle;
            }
            return 0;
        }
        catch { document.Dispose(); return -6; }
    }

    [UnmanagedCallersOnly(EntryPoint = "Release", CallConvs = [typeof(CallConvCdecl)])]
    public static int Release(nint handle)
    {
        lock (Gate)
        {
            if (!Documents.Remove(handle, out State? state)) return -1;
            state.Dispose();
        }
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "ReadAi", CallConvs = [typeof(CallConvCdecl)])]
    public static int ReadAi(nint handle, float* output, int capacity)
    {
        if (output == null || capacity != GeneratedAircraftAIParameters.FieldCount) return -1;
        try
        {
            lock (Gate)
                return Documents.TryGetValue(handle, out State? state) ? state.Read(new Span<float>(output, capacity)) : -1;
        }
        catch { return -6; }
    }

    [UnmanagedCallersOnly(EntryPoint = "ReadNumber", CallConvs = [typeof(CallConvCdecl)])]
    public static int ReadNumber(nint handle, byte* key, int length, float* output)
    {
        if (key == null || length <= 0 || output == null) return -1;
        try
        {
            lock (Gate)
            {
                if (!Documents.TryGetValue(handle, out State? state)) return -1;
                if (!state.Document.RootElement.TryGetProperty(new ReadOnlySpan<byte>(key, length), out var value)) return -3;
                if (value.ValueKind != JsonValueKind.Number || !value.TryGetSingle(out float number) || !float.IsFinite(number)) return -5;
                *output = number;
                return 0;
            }
        }
        catch { return -6; }
    }

    [UnmanagedCallersOnly(EntryPoint = "Collect", CallConvs = [typeof(CallConvCdecl)])]
    public static void Collect()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
    }
}
