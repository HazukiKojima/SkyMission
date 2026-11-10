using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using REDox.Json;
using REDoxWrapper.Generated;

namespace REDoxWrapper;

public static unsafe class REDoxApi
{
    private const int InvalidArguments = -1;
    private const int InvalidJson = -2;
    private const int InvalidSchema = -3;
    private const int InvalidValue = -5;
    private const int UnexpectedFailure = -6;

    // ハンドルには生ポインターではなく管理テーブルのIDを使う
    // 取得と解放を同じロックで保護し、解放済み文書へのアクセスを防ぐ
    private static readonly object DocumentLock = new();
    private static readonly Dictionary<nint, ParsedJsonDocument> Documents = new();
    private static long NextDocumentHandle;
    private static readonly NumericParameterLayout FlightLayout = new(
        GeneratedAircraftFlightParameters.FieldNames, GeneratedAircraftFlightParameters.FieldLengths);
    private static readonly NumericParameterLayout AiLayout = new(
        GeneratedAircraftAIParameters.FieldNames, GeneratedAircraftAIParameters.FieldLengths);
    private static readonly NumericParameterLayout OceanLayout = new(
        GeneratedOceanParameters.FieldNames, GeneratedOceanParameters.FieldLengths);
    private static readonly NumericParameterLayout SkyLayout = new(
        GeneratedSkyParameters.FieldNames, GeneratedSkyParameters.FieldLengths);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxCreateDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxCreateDocument(byte* jsonUtf8, int jsonLength, nint* outputHandle)
    {
        if (jsonUtf8 == null || jsonLength <= 0 || outputHandle == null)
        {
            return InvalidArguments;
        }

        *outputHandle = 0;
        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
        }
        catch
        {
            return InvalidJson;
        }

        try
        {
            lock (DocumentLock)
            {
                nint handle;
                do
                {
                    handle = (nint)Interlocked.Increment(ref NextDocumentHandle);
                }
                while (handle == 0 || Documents.ContainsKey(handle));

                Documents.Add(handle, new ParsedJsonDocument(document));
                *outputHandle = handle;
            }
            return 0;
        }
        catch
        {
            document.Dispose();
            return UnexpectedFailure;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxReleaseDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxReleaseDocument(nint handle)
    {
        if (handle == 0)
        {
            return InvalidArguments;
        }

        try
        {
            lock (DocumentLock)
            {
                if (!Documents.Remove(handle, out ParsedJsonDocument? document))
                {
                    return InvalidArguments;
                }

                document.Dispose();
                return 0;
            }
        }
        catch { return UnexpectedFailure; }
    }

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetAircraftFlightParameters",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetAircraftFlightParameters(
        byte* jsonUtf8,
        int jsonLength,
        float* output,
        int outputCapacity)
    {
        return ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            FlightLayout);
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetAircraftFlightParametersFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetAircraftFlightParametersFromDocument(nint handle, float* output, int outputCapacity) =>
        ReadParametersFromDocument(handle, output, outputCapacity,
            FlightLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetAircraftAIParameters", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetAircraftAIParameters(byte* jsonUtf8, int jsonLength, float* output, int outputCapacity) =>
        ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            AiLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetAircraftAIParametersFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetAircraftAIParametersFromDocument(nint handle, float* output, int outputCapacity) =>
        ReadParametersFromDocument(handle, output, outputCapacity,
            AiLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetOceanParameters", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetOceanParameters(byte* jsonUtf8, int jsonLength, float* output, int outputCapacity) =>
        ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            OceanLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetOceanParametersFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetOceanParametersFromDocument(nint handle, float* output, int outputCapacity) =>
        ReadParametersFromDocument(handle, output, outputCapacity,
            OceanLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetSkyParameters", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetSkyParameters(byte* jsonUtf8, int jsonLength, float* output, int outputCapacity) =>
        ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            SkyLayout);

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetSkyParametersFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetSkyParametersFromDocument(nint handle, float* output, int outputCapacity) =>
        ReadParametersFromDocument(handle, output, outputCapacity,
            SkyLayout);

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetRetroFilmParameters",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetRetroFilmParameters(
        byte* jsonUtf8,
        int jsonLength,
        float* output,
        int outputCapacity)
    {
        if (jsonUtf8 == null || jsonLength <= 0 || output == null || outputCapacity != 52)
        {
            return InvalidArguments;
        }

        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
        }
        catch
        {
            return InvalidJson;
        }

        using (document)
        {
            return ReadRetroFilmParameters(document, output, outputCapacity);
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetRetroFilmParametersFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetRetroFilmParametersFromDocument(nint handle, float* output, int outputCapacity)
    {
        if (handle == 0 || output == null || outputCapacity != 52) return InvalidArguments;
        lock (DocumentLock)
        {
            return Documents.TryGetValue(handle, out ParsedJsonDocument? document)
                ? ReadRetroFilmParameters(document.Document, output, outputCapacity)
                : InvalidArguments;
        }
    }

    private static int ReadRetroFilmParameters(JsonDocument document, float* output, int outputCapacity)
    {
        const int parameterCount = 52;
        try
        {
            REDox.DElement root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object ||
                !TryGetObject(root, "colorGrading", out REDox.DElement colorGrading) ||
                !TryGetObject(colorGrading, "blackLift", out REDox.DElement blackLift) ||
                !TryGetObject(colorGrading, "highlight", out REDox.DElement highlight) ||
                !TryGetObject(root, "vignette", out REDox.DElement vignette) ||
                !TryGetObject(root, "jitter", out REDox.DElement jitter) ||
                !TryGetObject(root, "grain", out REDox.DElement grain) ||
                !TryGetObject(root, "dust", out REDox.DElement dust) ||
                !TryGetObject(root, "scratch", out REDox.DElement scratch) ||
                !TryGetObject(root, "flicker", out REDox.DElement flicker)) return InvalidSchema;

            Span<float> values = stackalloc float[parameterCount];
            int index = 0;
            bool valid =
                AppendNumber(colorGrading, "desaturation", values, ref index) && AppendArray(colorGrading, "tint", 3, values, ref index) &&
                AppendNumber(colorGrading, "gain", values, ref index) && AppendArray(colorGrading, "offset", 3, values, ref index) &&
                AppendArray(blackLift, "color", 3, values, ref index) && AppendNumber(blackLift, "amount", values, ref index) &&
                AppendNumber(highlight, "start", values, ref index) && AppendNumber(highlight, "end", values, ref index) &&
                AppendNumber(highlight, "strength", values, ref index) && AppendNumber(highlight, "scale", values, ref index) &&
                AppendArray(highlight, "add", 3, values, ref index) && AppendArray(root, "luminanceWeights", 3, values, ref index) &&
                AppendNumber(vignette, "innerRadius", values, ref index) && AppendNumber(vignette, "outerRadius", values, ref index) &&
                AppendNumber(vignette, "minBrightness", values, ref index) && AppendArray(jitter, "amplitude", 2, values, ref index) &&
                AppendArray(jitter, "frequency", 2, values, ref index) && AppendNumber(grain, "fps", values, ref index) &&
                AppendNumber(grain, "intensity", values, ref index) && AppendNumber(dust, "cellSize", values, ref index) &&
                AppendNumber(dust, "fps", values, ref index) && AppendNumber(dust, "threshold", values, ref index) &&
                AppendArray(dust, "radius", 2, values, ref index) && AppendNumber(dust, "intensity", values, ref index) &&
                AppendNumber(scratch, "columnWidth", values, ref index) && AppendNumber(scratch, "fps", values, ref index) &&
                AppendNumber(scratch, "threshold", values, ref index) && AppendArray(scratch, "width", 2, values, ref index) &&
                AppendNumber(scratch, "edgeSoftness", values, ref index) && AppendNumber(scratch, "verticalFadeMin", values, ref index) &&
                AppendNumber(scratch, "intensity", values, ref index) && AppendNumber(flicker, "fps", values, ref index) &&
                AppendNumber(flicker, "amplitude", values, ref index) && AppendArray(root, "overlayColor", 3, values, ref index);
            if (!valid || index != parameterCount - 2) return InvalidValue;
            values[index++] = 0.0f;
            values[index++] = 0.0f;
            values.CopyTo(new Span<float>(output, outputCapacity));
            return index;
        }
        catch { return UnexpectedFailure; }
    }

    private static bool TryGetObject(REDox.DElement parent, string name, out REDox.DElement value)
    {
        return parent.TryGetProperty(name, out value) && value.ValueKind == JsonValueKind.Object;
    }

    private static bool AppendNumber(REDox.DElement parent, string name, Span<float> values, ref int index)
    {
        if (!parent.TryGetProperty(name, out REDox.DElement value) ||
            value.ValueKind != JsonValueKind.Number ||
            !value.TryGetSingle(out float number) || !float.IsFinite(number))
        {
            return false;
        }

        values[index++] = number;
        return true;
    }

    private static bool AppendArray(REDox.DElement parent, string name, int length, Span<float> values, ref int index)
    {
        if (!parent.TryGetProperty(name, out REDox.DElement value) ||
            value.ValueKind != JsonValueKind.Array || value.GetArrayLength() != length)
        {
            return false;
        }

        foreach (REDox.DElement element in value.EnumerateArray())
        {
            if (element.ValueKind != JsonValueKind.Number ||
                !element.TryGetSingle(out float number) || !float.IsFinite(number))
            {
                return false;
            }

            values[index++] = number;
        }

        return true;
    }

    private static int ReadParameters(
        byte* jsonUtf8, int jsonLength, float* output, int outputCapacity,
        NumericParameterLayout layout)
    {
        if (jsonUtf8 == null || jsonLength <= 0 || output == null || outputCapacity != layout.ValueCount)
            return InvalidArguments;
        JsonDocument document;
        try { document = JsonDocument.Parse(new ReadOnlySpan<byte>(jsonUtf8, jsonLength)); }
        catch { return InvalidJson; }
        using (document)
        {
            try
            {
                // エラー時に呼び出し元の出力を途中まで書き換えない
                Span<float> values = outputCapacity <= 256 ? stackalloc float[outputCapacity] : new float[outputCapacity];
                int result = layout.Read(document, values);
                if (result > 0) values.CopyTo(new Span<float>(output, outputCapacity));
                return result;
            }
            catch { return UnexpectedFailure; }
        }
    }

    private static int ReadParametersFromDocument(
        nint handle, float* output, int outputCapacity, NumericParameterLayout layout)
    {
        if (handle == 0 || output == null || outputCapacity != layout.ValueCount) return InvalidArguments;
        try
        {
            lock (DocumentLock)
            {
                return Documents.TryGetValue(handle, out ParsedJsonDocument? document)
                    ? document.ReadParameters(layout, new Span<float>(output, outputCapacity))
                    : InvalidArguments;
            }
        }
        catch { return UnexpectedFailure; }
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetFloatArrayFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetFloatArrayFromDocument(
        nint handle, byte* keyUtf8, int keyLength, float* output, int outputCapacity)
    {
        if (handle == 0 || keyUtf8 == null || keyLength <= 0 || output == null || outputCapacity <= 0)
            return InvalidArguments;
        try
        {
            ReadOnlySpan<byte> key = new(keyUtf8, keyLength);
            lock (DocumentLock)
            {
                return Documents.TryGetValue(handle, out ParsedJsonDocument? document)
                    ? ReadFloatArray(document.Document, key, output, outputCapacity)
                    : InvalidArguments;
            }
        }
        catch { return UnexpectedFailure; }
    }

    private static int ReadFloatArray(JsonDocument document, ReadOnlySpan<byte> key, float* output, int outputCapacity)
    {
        int lookup = FindValue(document, key, out REDox.DElement value);
        if (lookup < 0) return lookup;
        Span<float> values = outputCapacity <= 256 ? stackalloc float[outputCapacity] : new float[outputCapacity];
        if (value.ValueKind == JsonValueKind.Number)
        {
            if (!value.TryGetSingle(out float number) || !float.IsFinite(number)) return InvalidValue;
            values[0] = number;
            values[..1].CopyTo(new Span<float>(output, outputCapacity));
            return 1;
        }
        if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() > outputCapacity) return InvalidValue;
        int index = 0;
        foreach (REDox.DElement element in value.EnumerateArray())
        {
            if (!element.TryGetSingle(out float number) || !float.IsFinite(number)) return InvalidValue;
            values[index++] = number;
        }
        values[..index].CopyTo(new Span<float>(output, outputCapacity));
        return index;
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetStringFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetStringFromDocument(
        nint handle, byte* keyUtf8, int keyLength, byte* output, int outputCapacity)
    {
        if (handle == 0 || keyUtf8 == null || keyLength <= 0 || output == null || outputCapacity <= 0)
            return InvalidArguments;
        try
        {
            ReadOnlySpan<byte> key = new(keyUtf8, keyLength);
            lock (DocumentLock)
            {
                return Documents.TryGetValue(handle, out ParsedJsonDocument? document)
                    ? ReadString(document.Document, key, output, outputCapacity)
                    : InvalidArguments;
            }
        }
        catch { return UnexpectedFailure; }
    }

    private static int ReadString(JsonDocument document, ReadOnlySpan<byte> key, byte* output, int outputCapacity)
    {
        int lookup = FindValue(document, key, out REDox.DElement value);
        if (lookup < 0) return lookup;
        if (value.ValueKind != JsonValueKind.String) return InvalidValue;
        string? result = value.GetString();
        if (result == null) return InvalidValue;
        int requiredLength = Encoding.UTF8.GetByteCount(result);
        if (requiredLength > outputCapacity) return InvalidValue;
        Encoding.UTF8.GetBytes(result.AsSpan(), new Span<byte>(output, outputCapacity));
        return requiredLength;
    }

    // 既存のルートキーを維持し、配列内の値にはJSON Pointerを使う。
    private static int FindValue(JsonDocument document, ReadOnlySpan<byte> key, out REDox.DElement value)
    {
        value = document.RootElement;
        string path = Encoding.UTF8.GetString(key);
        if (!path.StartsWith('/'))
            return value.TryGetProperty(key, out value) ? 0 : InvalidSchema;
        foreach (string encoded in path[1..].Split('/'))
        {
            string part = encoded.Replace("~1", "/").Replace("~0", "~");
            if (value.ValueKind == JsonValueKind.Object)
            {
                if (!value.TryGetProperty(part, out value)) return InvalidSchema;
            }
            else if (value.ValueKind == JsonValueKind.Array)
            {
                if (!int.TryParse(part, out int index) || index < 0 || index >= value.GetArrayLength())
                    return InvalidValue;
                int current = 0;
                foreach (REDox.DElement element in value.EnumerateArray())
                    if (current++ == index) { value = element; break; }
            }
            else return InvalidValue;
        }
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetArrayLengthFromDocument", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetArrayLengthFromDocument(nint handle, byte* keyUtf8, int keyLength)
    {
        if (handle == 0 || keyUtf8 == null || keyLength <= 0) return InvalidArguments;
        try
        {
            lock (DocumentLock)
            {
                if (!Documents.TryGetValue(handle, out ParsedJsonDocument? document)) return InvalidArguments;
                int lookup = FindValue(document.Document, new ReadOnlySpan<byte>(keyUtf8, keyLength), out var value);
                if (lookup < 0) return lookup;
                return value.ValueKind == JsonValueKind.Array ? value.GetArrayLength() : InvalidValue;
            }
        }
        catch { return UnexpectedFailure; }
    }

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetFloatArray",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetFloatArray(
        byte* jsonUtf8,
        int jsonLength,
        byte* keyUtf8,
        int keyLength,
        float* output,
        int outputCapacity)
    {
        if (jsonUtf8 == null || jsonLength <= 0 ||
            keyUtf8 == null || keyLength <= 0 ||
            output == null || outputCapacity <= 0)
        {
            return InvalidArguments;
        }

        try
        {
            string key = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(keyUtf8, keyLength));
            JsonDocument document;
            try
            {
                document = JsonDocument.Parse(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            }
            catch
            {
                return InvalidJson;
            }

            using (document)
            {
                if (!document.RootElement.TryGetProperty(key, out var value))
                {
                    return InvalidSchema;
                }

                if (value.ValueKind == JsonValueKind.Number)
                {
                    if (outputCapacity < 1 || !value.TryGetSingle(out output[0]) ||
                        !float.IsFinite(output[0]))
                    {
                        return InvalidValue;
                    }
                    return 1;
                }

                if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() > outputCapacity)
                {
                    return InvalidValue;
                }

                int index = 0;
                foreach (var element in value.EnumerateArray())
                {
                    if (!element.TryGetSingle(out output[index]) || !float.IsFinite(output[index]))
                    {
                        return InvalidValue;
                    }
                    index++;
                }
                return index;
            }
        }
        catch
        {
            return UnexpectedFailure;
        }
    }

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetString",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetString(
        byte* jsonUtf8,
        int jsonLength,
        byte* keyUtf8,
        int keyLength,
        byte* output,
        int outputCapacity)
    {
        if (jsonUtf8 == null || jsonLength <= 0 ||
            keyUtf8 == null || keyLength <= 0 ||
            output == null || outputCapacity <= 0)
        {
            return InvalidArguments;
        }

        try
        {
            string key = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(keyUtf8, keyLength));

            JsonDocument document;
            try
            {
                document = JsonDocument.Parse(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            }
            catch
            {
                return InvalidJson;
            }

            using (document)
            {
                var root = document.RootElement;

                if (!root.TryGetProperty(key, out var value))
                {
                    return InvalidSchema;
                }

                string? result = value.GetString();

                if (result == null)
                {
                    return InvalidValue;
                }

                int requiredLength = Encoding.UTF8.GetByteCount(result);

                if (requiredLength > outputCapacity)
                {
                    return InvalidValue;
                }

                Encoding.UTF8.GetBytes(
                    result.AsSpan(),
                    new Span<byte>(output, outputCapacity));

                return requiredLength;
            }
        }
        catch
        {
            return UnexpectedFailure;
        }
    }
}
