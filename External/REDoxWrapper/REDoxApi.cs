using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using REDox.Json;
using REDoxWrapper.Generated;

namespace REDoxWrapper;

public static unsafe class REDoxApi
{
    private const int InvalidArguments = -1;
    private const int InvalidJson = -2;
    private const int InvalidSchema = -3;
    private const int DeserializationFailed = -4;
    private const int InvalidValue = -5;
    private const int UnexpectedFailure = -6;

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetAircraftFlightParameters",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    [DynamicDependency(DynamicallyAccessedMemberTypes.PublicProperties, typeof(GeneratedAircraftFlightParameters))]
    public static int MyRedoxGetAircraftFlightParameters(
        byte* jsonUtf8,
        int jsonLength,
        float* output,
        int outputCapacity)
    {
        return ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            GeneratedAircraftFlightParameters.FieldNames,
            GeneratedAircraftFlightParameters.FieldLengths,
            json => JsonSerializer.Deserialize<GeneratedAircraftFlightParameters>(json)?.ToFloatArray());
    }

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetOceanParameters", CallConvs = new[] { typeof(CallConvCdecl) })]
    [DynamicDependency(DynamicallyAccessedMemberTypes.PublicProperties, typeof(GeneratedOceanParameters))]
    public static int MyRedoxGetOceanParameters(byte* jsonUtf8, int jsonLength, float* output, int outputCapacity) =>
        ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            GeneratedOceanParameters.FieldNames, GeneratedOceanParameters.FieldLengths,
            json => JsonSerializer.Deserialize<GeneratedOceanParameters>(json)?.ToFloatArray());

    [UnmanagedCallersOnly(EntryPoint = "MyRedoxGetSkyParameters", CallConvs = new[] { typeof(CallConvCdecl) })]
    [DynamicDependency(DynamicallyAccessedMemberTypes.PublicProperties, typeof(GeneratedSkyParameters))]
    public static int MyRedoxGetSkyParameters(byte* jsonUtf8, int jsonLength, float* output, int outputCapacity) =>
        ReadParameters(jsonUtf8, jsonLength, output, outputCapacity,
            GeneratedSkyParameters.FieldNames, GeneratedSkyParameters.FieldLengths,
            json => JsonSerializer.Deserialize<GeneratedSkyParameters>(json)?.ToFloatArray());

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxGetRetroFilmParameters",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxGetRetroFilmParameters(
        byte* jsonUtf8,
        int jsonLength,
        float* output,
        int outputCapacity)
    {
        const int parameterCount = 52;
        if (jsonUtf8 == null || jsonLength <= 0 || output == null || outputCapacity != parameterCount)
        {
            return InvalidArguments;
        }

        JsonDocument document;
        try
        {
            string json = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            document = JsonDocument.Parse(json);
        }
        catch
        {
            return InvalidJson;
        }

        using (document)
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
                !TryGetObject(root, "flicker", out REDox.DElement flicker))
            {
                return InvalidSchema;
            }

            Span<float> values = stackalloc float[parameterCount];
            int index = 0;
            bool valid =
                AppendNumber(colorGrading, "desaturation", values, ref index) &&
                AppendArray(colorGrading, "tint", 3, values, ref index) &&
                AppendNumber(colorGrading, "gain", values, ref index) &&
                AppendArray(colorGrading, "offset", 3, values, ref index) &&
                AppendArray(blackLift, "color", 3, values, ref index) &&
                AppendNumber(blackLift, "amount", values, ref index) &&
                AppendNumber(highlight, "start", values, ref index) &&
                AppendNumber(highlight, "end", values, ref index) &&
                AppendNumber(highlight, "strength", values, ref index) &&
                AppendNumber(highlight, "scale", values, ref index) &&
                AppendArray(highlight, "add", 3, values, ref index) &&
                AppendArray(root, "luminanceWeights", 3, values, ref index) &&
                AppendNumber(vignette, "innerRadius", values, ref index) &&
                AppendNumber(vignette, "outerRadius", values, ref index) &&
                AppendNumber(vignette, "minBrightness", values, ref index) &&
                AppendArray(jitter, "amplitude", 2, values, ref index) &&
                AppendArray(jitter, "frequency", 2, values, ref index) &&
                AppendNumber(grain, "fps", values, ref index) &&
                AppendNumber(grain, "intensity", values, ref index) &&
                AppendNumber(dust, "cellSize", values, ref index) &&
                AppendNumber(dust, "fps", values, ref index) &&
                AppendNumber(dust, "threshold", values, ref index) &&
                AppendArray(dust, "radius", 2, values, ref index) &&
                AppendNumber(dust, "intensity", values, ref index) &&
                AppendNumber(scratch, "columnWidth", values, ref index) &&
                AppendNumber(scratch, "fps", values, ref index) &&
                AppendNumber(scratch, "threshold", values, ref index) &&
                AppendArray(scratch, "width", 2, values, ref index) &&
                AppendNumber(scratch, "edgeSoftness", values, ref index) &&
                AppendNumber(scratch, "verticalFadeMin", values, ref index) &&
                AppendNumber(scratch, "intensity", values, ref index) &&
                AppendNumber(flicker, "fps", values, ref index) &&
                AppendNumber(flicker, "amplitude", values, ref index) &&
                AppendArray(root, "overlayColor", 3, values, ref index);

            if (!valid || index != parameterCount - 2)
            {
                return InvalidValue;
            }

            values[index++] = 0.0f;
            values[index++] = 0.0f;
            values.CopyTo(new Span<float>(output, outputCapacity));
            return index;
        }
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
        string[] fieldNames, int[] fieldLengths, Func<string, float[]?> deserialize)
    {
        if (jsonUtf8 == null || jsonLength <= 0 || output == null ||
            outputCapacity <= 0 || fieldNames.Length != fieldLengths.Length ||
            fieldLengths.Sum() != outputCapacity)
        {
            return InvalidArguments;
        }

        string json;
        try
        {
            json = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
        }
        catch
        {
            return InvalidJson;
        }

        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(json);
        }
        catch
        {
            return InvalidJson;
        }

        using (document)
        {
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                return InvalidSchema;
            }

            for (int index = 0; index < fieldNames.Length; index++)
            {
                if (!root.TryGetProperty(fieldNames[index], out var value) ||
                    !HasFiniteNumbers(value, fieldLengths[index]))
                {
                    return InvalidValue;
                }
            }

            float[]? values;
            try
            {
                values = deserialize(json);
            }
            catch
            {
                return DeserializationFailed;
            }

            if (values == null || values.Length != outputCapacity || values.Any(value => !float.IsFinite(value)))
            {
                return InvalidValue;
            }

            values.AsSpan().CopyTo(new Span<float>(output, outputCapacity));
            return values.Length;
        }
    }

    private static bool HasFiniteNumbers(REDox.DElement value, int length)
    {
        if (length == 1)
        {
            if (value.ValueKind != JsonValueKind.Number || !value.TryGetSingle(out float number)) return false;
            return float.IsFinite(number);
        }

        if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() != length) return false;
        foreach (var element in value.EnumerateArray())
        {
            if (element.ValueKind != JsonValueKind.Number || !element.TryGetSingle(out float number) ||
                !float.IsFinite(number)) return false;
        }
        return true;
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
            string json = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            string key = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(keyUtf8, keyLength));
            JsonDocument document;
            try
            {
                document = JsonDocument.Parse(json);
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
            string json = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(jsonUtf8, jsonLength));

            string key = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(keyUtf8, keyLength));

            JsonDocument document;
            try
            {
                document = JsonDocument.Parse(json);
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