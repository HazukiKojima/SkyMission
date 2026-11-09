using System.Text;
using System.Runtime.CompilerServices;
using REDox.Json;
using REDoxWrapper;
using REDoxWrapper.Generated;

internal static unsafe class Program
{
    private static void Require(bool value, string message)
    {
        if (!value) throw new InvalidOperationException(message);
    }

    private static nint Create(byte[] bytes)
    {
        nint handle = 0;
        delegate* unmanaged[Cdecl]<byte*, int, nint*, int> create = &REDoxApi.MyRedoxCreateDocument;
        fixed (byte* input = bytes) Require(create(input, bytes.Length, &handle) == 0, "Create document failed");
        return handle;
    }

    private static int ReadAi(nint handle, float[] output)
    {
        delegate* unmanaged[Cdecl]<nint, float*, int, int> read = &REDoxApi.MyRedoxGetAircraftAIParametersFromDocument;
        fixed (float* values = output) return read(handle, values, output.Length);
    }

    private static int Release(nint handle)
    {
        delegate* unmanaged[Cdecl]<nint, int> release = &REDoxApi.MyRedoxReleaseDocument;
        return release(handle);
    }

    private static int ReadAiBytes(byte[] bytes, float[] output)
    {
        delegate* unmanaged[Cdecl]<byte*, int, float*, int, int> read = &REDoxApi.MyRedoxGetAircraftAIParameters;
        fixed (byte* input = bytes)
        fixed (float* values = output) return read(input, bytes.Length, values, output.Length);
    }

    private static void Main(string[] args)
    {
        var layout = new NumericParameterLayout(["a", "vector"], [1, 3]);
        foreach (string json in new[] {
            "{\"a\":1,\"vector\":[2,3,4]}",
            "{\"unused\":null,\"vector\":[2,3,4],\"a\":1}",
            "{\"a\":1,\"a\":null,\"vector\":[2,3,4]}",
            "{\"\\u0061\":1,\"vector\":[2,3,4]}" })
        {
            using var document = JsonDocument.Parse(json);
            float[] output = new float[4];
            Require(layout.Read(document, output) == 4 && output.SequenceEqual(new float[] { 1, 2, 3, 4 }), "Layout order/duplicate/escaped property changed");
            Require(output[0] == document.RootElement.GetProperty("a").GetSingle(), "Duplicate handling differs from the installed RE:DOX API");
            using var cached = new ParsedJsonDocument(JsonDocument.Parse(json));
            Require(cached.ReadParameters(layout, output) == 4, "First cached read failed");
            output.AsSpan().Fill(-1);
            Require(cached.ReadParameters(layout, output) == 4 && output[0] == 1 && output[3] == 4, "Caller mutated cached parameters");
        }
        foreach (string json in new[] {
            "{}", "{\"a\":null,\"vector\":[2,3,4]}", "{\"a\":\"1\",\"vector\":[2,3,4]}",
            "{\"a\":true,\"vector\":[2,3,4]}", "{\"a\":1,\"vector\":[2,3]}",
            "{\"a\":1,\"vector\":[2,3,4,5]}", "{\"a\":1,\"vector\":[2,null,4]}",
            "{\"a\":3.5e38,\"vector\":[2,3,4]}", "{\"a\":null,\"a\":1,\"vector\":[2,3,4]}" })
        {
            using var cached = new ParsedJsonDocument(JsonDocument.Parse(json));
            float[] output = [-1, -1, -1, -1];
            Require(cached.ReadParameters(layout, output) == -5 && output.All(value => value == -1), "Invalid layout partially wrote output");
            Require(cached.ReadParameters(layout, output) == -5, "Failed layout was cached as valid");
        }
        using (var document = JsonDocument.Parse("[]")) Require(layout.Read(document, new float[4]) == -3, "Root type validation changed");

        byte[] source = File.ReadAllBytes(Path.Combine(args[0], "Assets/Aircraft/F4U4Corsair/aiParameters.json"));
        using var expectedDocument = JsonDocument.Parse(source);
        float[] expected = GeneratedAircraftAIParameters.FieldNames.Select(name => expectedDocument.RootElement.GetProperty(name).GetSingle()).ToArray();
        nint handle = Create((byte[])source.Clone());
        float[] parameters = new float[expected.Length];
        Require(ReadAiBytes(source, parameters) == expected.Length && parameters.SequenceEqual(expected), "Legacy byte-input API changed");
        foreach (var invalid in new[] { ("{", -2), ("{}", -5), ("[]", -3) })
        {
            Array.Fill(parameters, -1);
            Require(ReadAiBytes(Encoding.UTF8.GetBytes(invalid.Item1), parameters) == invalid.Item2 &&
                parameters.All(value => value == -1), "Legacy byte-input error or atomic output changed");
        }
        Require(ReadAiBytes(source, new float[expected.Length - 1]) == -1, "Legacy byte-input capacity validation changed");
        Require(ReadAi(handle, parameters) == expected.Length && parameters.SequenceEqual(expected), "Generated C++ field layout changed");
        Array.Fill(parameters, -1);
        Require(ReadAi(handle, parameters) == expected.Length && parameters.SequenceEqual(expected), "Export cache changed values");
        Require(ReadAi(handle, new float[expected.Length - 1]) == -1, "Capacity validation changed");
        Require(Release(handle) == 0 && Release(handle) == -1 && ReadAi(handle, parameters) == -1, "Released handle remained usable");

        string originalText = Encoding.UTF8.GetString(source).Trim();
        foreach (string duplicate in new[] {
            "{\"cruiseSpeed\":777," + originalText[1..],
            originalText[..^1] + ",\"cruiseSpeed\":777}" })
        {
            using var reference = JsonDocument.Parse(duplicate);
            float[] referenceValues = GeneratedAircraftAIParameters.FieldNames.Select(name => reference.RootElement.GetProperty(name).GetSingle()).ToArray();
            handle = Create(Encoding.UTF8.GetBytes(duplicate));
            Require(ReadAi(handle, parameters) == expected.Length && parameters.SequenceEqual(referenceValues), "Large-schema duplicate semantics changed");
            Require(Release(handle) == 0, "Duplicate fixture release failed");
        }

        // 入力バッファの寿命を文書と分離し、UTF-8直接解析がC++側の一時領域を参照しないことを確認
        byte[] temporary = (byte[])source.Clone();
        handle = Create(temporary);
        Array.Clear(temporary);
        Require(ReadAi(handle, parameters) == expected.Length && parameters.SequenceEqual(expected), "Document borrowed the caller's UTF-8 buffer");
        byte[] key = Encoding.UTF8.GetBytes("cruiseSpeed");
        float number = 0;
        delegate* unmanaged[Cdecl]<nint, byte*, int, float*, int, int> getFloat = &REDoxApi.MyRedoxGetFloatArrayFromDocument;
        fixed (byte* pointer = key) Require(getFloat(handle, pointer, key.Length, &number, 1) == 1 && number == expectedDocument.RootElement.GetProperty("cruiseSpeed").GetSingle(), "UTF-8 field lookup changed");
        Require(Release(handle) == 0, "Release failed");

        handle = Create(Encoding.UTF8.GetBytes("{\"巡航速度\":800.25,\"名前\":\"紫電改\"}"));
        key = Encoding.UTF8.GetBytes("巡航速度");
        fixed (byte* pointer = key) Require(getFloat(handle, pointer, key.Length, &number, 1) == 1 && number == 800.25f, "Unicode UTF-8 numeric key changed");
        key = Encoding.UTF8.GetBytes("名前");
        byte[] text = new byte[64];
        int textLength;
        delegate* unmanaged[Cdecl]<nint, byte*, int, byte*, int, int> getString = &REDoxApi.MyRedoxGetStringFromDocument;
        fixed (byte* pointer = key)
        fixed (byte* output = text) textLength = getString(handle, pointer, key.Length, output, text.Length);
        Require(textLength > 0 && Encoding.UTF8.GetString(text, 0, textLength) == "紫電改", "Unicode UTF-8 string lookup changed");
        Require(Release(handle) == 0, "Unicode document release failed");

        Parallel.For(0, 100, _ =>
        {
            nint own = Create(source);
            float[] values = new float[expected.Length];
            Require(ReadAi(own, values) == values.Length && values.SequenceEqual(expected), "Concurrent document/cache mismatch");
            Require(Release(own) == 0, "Concurrent release failed");
        });
        Console.WriteLine("PASS: field order, first duplicate, arrays, finite values, atomic errors, UTF-8 ownership, ABI, cache lifetime and concurrency");
    }
}
