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
        if (jsonUtf8 == null || jsonLength <= 0 || output == null ||
            outputCapacity != GeneratedAircraftFlightParameters.FieldCount)
        {
            return -1;
        }

        try
        {
            string json = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object ||
                root.EnumerateObject().Count() != GeneratedAircraftFlightParameters.FieldCount)
            {
                return -1;
            }

            foreach (string fieldName in GeneratedAircraftFlightParameters.FieldNames)
            {
                if (!root.TryGetProperty(fieldName, out var value) ||
                    value.ValueKind != JsonValueKind.Number ||
                    !value.TryGetSingle(out var number))
                {
                    return -1;
                }
                if (!float.IsFinite(number)) return -1;
            }

            GeneratedAircraftFlightParameters? parameters =
                JsonSerializer.Deserialize<GeneratedAircraftFlightParameters>(json);
            if (parameters == null)
            {
                return -1;
            }

            float[] values = parameters.ToFloatArray();
            if (values.Length != outputCapacity || values.Any(value => !float.IsFinite(value)))
            {
                return -1;
            }

            values.AsSpan().CopyTo(new Span<float>(output, outputCapacity));
            return values.Length;
        }
        catch
        {
            return -1;
        }
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
            return -1;
        }

        try
        {
            string json = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            string key = Encoding.UTF8.GetString(new ReadOnlySpan<byte>(keyUtf8, keyLength));
            using var document = JsonDocument.Parse(json);

            if (!document.RootElement.TryGetProperty(key, out var value))
            {
                return -1;
            }

            if (value.ValueKind == JsonValueKind.Number)
            {
                if (outputCapacity < 1 || !value.TryGetSingle(out output[0]))
                {
                    return -1;
                }
                return 1;
            }

            if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() > outputCapacity)
            {
                return -1;
            }

            int index = 0;
            foreach (var element in value.EnumerateArray())
            {
                if (!element.TryGetSingle(out output[index++]))
                {
                    return -1;
                }
            }
            return index;
        }
        catch
        {
            return -1;
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
            return -1;
        }

        try
        {
            string json = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(jsonUtf8, jsonLength));

            string key = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(keyUtf8, keyLength));

            using var document = JsonDocument.Parse(json);

            var root = document.RootElement;

            if (!root.TryGetProperty(key, out var value))
            {
                return -1;
            }

            string? result = value.GetString();

            if (result == null)
            {
                return -1;
            }

            int requiredLength = Encoding.UTF8.GetByteCount(result);

            if (requiredLength > outputCapacity)
            {
                return -2;
            }

            Encoding.UTF8.GetBytes(
                result.AsSpan(),
                new Span<byte>(output, outputCapacity));

            return requiredLength;
        }
        catch
        {
            return -1;
        }
    }
}