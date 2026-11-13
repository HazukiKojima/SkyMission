using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using REDoxWrapper.Generated;
#if STANDARD_JSON
using Document = System.Text.Json.JsonDocument;
#else
using REDox.Json;
using Document = REDox.Json.JsonDocument;
#endif

namespace JsonParserBenchmark;

// 両パーサーで同じハンドル管理・ロック・公開関数を使い、C++からの利用コストを含める
public static unsafe class Bridge
{
    private static readonly object Gate = new();
    private static readonly Dictionary<nint, Document> Documents = [];
    private static long s_nextHandle;

    [UnmanagedCallersOnly(EntryPoint = "Collect", CallConvs = [typeof(CallConvCdecl)])]
    public static void Collect()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
    }

    [UnmanagedCallersOnly(EntryPoint = "Create", CallConvs = [typeof(CallConvCdecl)])]
    public static int Create(byte* json, int length, nint* handle)
    {
        try { return Store(Document.Parse(Encoding.UTF8.GetString(new ReadOnlySpan<byte>(json, length))), handle); }
        catch { return -1; }
    }

    [UnmanagedCallersOnly(EntryPoint = "CreateUtf8", CallConvs = [typeof(CallConvCdecl)])]
    public static int CreateUtf8(byte* json, int length, nint* handle)
    {
        try
        {
#if STANDARD_JSON
            // C++側の入力が解放されても文書が有効であるよう、所有するbyte[]を渡す
            return Store(Document.Parse(new ReadOnlySpan<byte>(json, length).ToArray()), handle);
#else
            return Store(Document.Parse(new ReadOnlySpan<byte>(json, length)), handle);
#endif
        }
        catch { return -1; }
    }

    private static int Store(Document document, nint* output)
    {
        lock (Gate)
        {
            nint handle = (nint)Interlocked.Increment(ref s_nextHandle);
            Documents.Add(handle, document);
            *output = handle;
        }
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "Release", CallConvs = [typeof(CallConvCdecl)])]
    public static int Release(nint handle)
    {
        lock (Gate)
        {
            if (!Documents.Remove(handle, out Document? document)) return -1;
            document.Dispose();
        }
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "ReadAi", CallConvs = [typeof(CallConvCdecl)])]
    public static int ReadAi(nint handle, float* values, int capacity)
    {
        if (capacity != GeneratedAircraftAIParameters.FieldCount) return -1;
        lock (Gate)
        {
            if (!Documents.TryGetValue(handle, out Document? document)) return -1;
            int index = 0;
            foreach (string field in GeneratedAircraftAIParameters.FieldNames)
            {
                if (!document.RootElement.TryGetProperty(field, out var value) || !value.TryGetSingle(out float number)) return -1;
                values[index++] = number;
            }
        }
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "ReadNumber", CallConvs = [typeof(CallConvCdecl)])]
    public static int ReadNumber(nint handle, byte* key, int keyLength, float* output)
    {
        try
        {
            string field = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(key, keyLength));
            lock (Gate)
            {
                if (!Documents.TryGetValue(handle, out Document? document) ||
                    !document.RootElement.TryGetProperty(field, out var value) || !value.TryGetSingle(out float number)) return -1;
                *output = number;
            }
            return 0;
        }
        catch { return -1; }
    }
}
