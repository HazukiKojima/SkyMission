using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
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
}