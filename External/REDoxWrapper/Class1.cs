using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text;
using REDox.Json;
namespace REDoxWrapper;

public static unsafe class REDoxApi
{
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

    [UnmanagedCallersOnly(
        EntryPoint = "MySystemTextJsonGetString",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MySystemTextJsonGetString(
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

            using var document = System.Text.Json.JsonDocument.Parse(json);

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

    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxBenchmarkBatch",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxBenchmarkBatch(
        byte* jsonUtf8,
        int jsonLength,
        byte* keyUtf8,
        int keyLength,
        int iterations,
        byte* output,
        int outputCapacity,
        double* elapsedNanoseconds)
    {
        if (jsonUtf8 == null || jsonLength <= 0 ||
            keyUtf8 == null || keyLength <= 0 ||
            iterations <= 0 || output == null || outputCapacity <= 0 ||
            elapsedNanoseconds == null)
        {
            return -1;
        }

        *elapsedNanoseconds = 0;
        try
        {
            string json = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            string key = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(keyUtf8, keyLength));
            string? result = null;

            long start = Stopwatch.GetTimestamp();
            for (int i = 0; i < iterations; i++)
            {
                using var document = JsonDocument.Parse(json);
                if (!document.RootElement.TryGetProperty(key, out var value))
                {
                    return -1;
                }

                result = value.GetString();
                if (result == null)
                {
                    return -1;
                }
            }
            *elapsedNanoseconds = Stopwatch.GetElapsedTime(start).TotalNanoseconds;

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

    [UnmanagedCallersOnly(
        EntryPoint = "MySystemTextJsonBenchmarkBatch",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MySystemTextJsonBenchmarkBatch(
        byte* jsonUtf8,
        int jsonLength,
        byte* keyUtf8,
        int keyLength,
        int iterations,
        byte* output,
        int outputCapacity,
        double* elapsedNanoseconds)
    {
        if (jsonUtf8 == null || jsonLength <= 0 ||
            keyUtf8 == null || keyLength <= 0 ||
            iterations <= 0 || output == null || outputCapacity <= 0 ||
            elapsedNanoseconds == null)
        {
            return -1;
        }

        *elapsedNanoseconds = 0;
        try
        {
            string json = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(jsonUtf8, jsonLength));
            string key = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(keyUtf8, keyLength));
            string? result = null;

            long start = Stopwatch.GetTimestamp();
            for (int i = 0; i < iterations; i++)
            {
                using var document = System.Text.Json.JsonDocument.Parse(json);
                if (!document.RootElement.TryGetProperty(key, out var value))
                {
                    return -1;
                }

                result = value.GetString();
                if (result == null)
                {
                    return -1;
                }
            }
            *elapsedNanoseconds = Stopwatch.GetElapsedTime(start).TotalNanoseconds;

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