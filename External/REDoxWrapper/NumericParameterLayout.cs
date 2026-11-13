using REDox.Json;

namespace REDoxWrapper;

// ゲーム固有の配置情報。RE:DOXの文書・トークン実装には手を加えない
internal sealed class NumericParameterLayout
{
    internal const int InvalidSchema = -3;
    internal const int InvalidValue = -5;
    private readonly Dictionary<string, int> _indices;
    private readonly int[] _offsets;
    private readonly int[] _lengths;
    internal int ValueCount { get; }

    internal NumericParameterLayout(string[] names, int[] lengths)
    {
        if (names.Length != lengths.Length) throw new ArgumentException("Parameter layout length mismatch");
        _indices = new(names.Length, StringComparer.Ordinal);
        _offsets = new int[names.Length];
        _lengths = (int[])lengths.Clone();
        int offset = 0;
        for (int index = 0; index < names.Length; ++index)
        {
            if (lengths[index] <= 0) throw new ArgumentException("Parameter field length must be positive");
            _indices.Add(names[index], index);
            _offsets[index] = offset;
            offset = checked(offset + lengths[index]);
        }
        ValueCount = offset;
    }

    internal int Read(JsonDocument document, Span<float> output)
    {
        if (output.Length != ValueCount) return InvalidValue;
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object) return InvalidSchema;
        Span<byte> seen = _lengths.Length <= 256 ? stackalloc byte[_lengths.Length] : new byte[_lengths.Length];
        seen.Clear();
        int found = 0;
        // 個別のプロパティ検索を繰り返さず、必要な項目を一度の走査で配置する
        foreach (var property in root.EnumerateObject())
        {
            if (!_indices.TryGetValue(property.Name, out int index) || seen[index] != 0) continue;
            // RE:DOX 1.0.3のTryGetPropertyと同じく、同名キーは最初の値を使う
            if (!ReadField(property.Value, output.Slice(_offsets[index], _lengths[index]))) return InvalidValue;
            seen[index] = 1;
            if (++found == _lengths.Length) return ValueCount;
        }
        return InvalidValue;
    }

    private static bool ReadField(REDox.DElement value, Span<float> output)
    {
        if (output.Length == 1)
        {
            if (value.ValueKind != JsonValueKind.Number ||
                !value.TryGetSingle(out float number) || !float.IsFinite(number)) return false;
            output[0] = number;
            return true;
        }
        if (value.ValueKind != JsonValueKind.Array || value.GetArrayLength() != output.Length) return false;
        int index = 0;
        foreach (var element in value.EnumerateArray())
        {
            if (element.ValueKind != JsonValueKind.Number ||
                !element.TryGetSingle(out float number) || !float.IsFinite(number)) return false;
            output[index++] = number;
        }
        return true;
    }
}
