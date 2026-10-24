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