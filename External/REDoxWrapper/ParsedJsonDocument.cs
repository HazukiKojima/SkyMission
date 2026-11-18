using REDox.Json;

namespace REDoxWrapper;

// 文書ハンドルと同じ寿命のキャッシュ。呼び出し側のDocumentLockで保護する
internal sealed class ParsedJsonDocument(JsonDocument document) : IDisposable
{
    internal JsonDocument Document { get; } = document;
    private readonly Dictionary<NumericParameterLayout, float[]> _parameters = [];

    internal int ReadParameters(NumericParameterLayout layout, Span<float> output)
    {
        if (output.Length != layout.ValueCount) return NumericParameterLayout.InvalidValue;

        // 初回の検証に成功した数値配列だけをキャッシュする
        if (!_parameters.TryGetValue(layout, out float[]? values))
        {
            values = new float[layout.ValueCount];
            int result = layout.Read(Document, values);

            if (result < 0) return result;

            _parameters.Add(layout, values);
        }

        // 呼び出し側にはコピーを渡してキャッシュを保護する
        values.AsSpan().CopyTo(output);
        return values.Length;
    }

    public void Dispose()
    {
        _parameters.Clear();
        Document.Dispose();
    }
}
