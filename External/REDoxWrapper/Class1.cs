using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using REDox.Json;

namespace REDoxWrapper;

public static unsafe class REDoxApi
{
    [UnmanagedCallersOnly(
        EntryPoint = "MyRedoxParse",
        CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int MyRedoxParse(byte* jsonUtf8, int jsonLength)
    {
        if (jsonUtf8 == null || jsonLength <= 0)
        {
            return -1;
        }

        try
        {
            string json = Encoding.UTF8.GetString(
                new ReadOnlySpan<byte>(jsonUtf8, jsonLength));

            using var document = JsonDocument.Parse(json);

            return 0;
        }
        catch
        {
            return -1;
        }
    }
}