namespace REDoxWrapper.Generated;

public sealed class GeneratedOceanParameters
{
    public const int FieldCount = 9;
    public static readonly string[] FieldNames =
    {
        "size",
        "waveLength",
        "waveSteepness",
        "normalUvScale",
        "wavePatternUvScale",
        "fogStartDistance",
        "fogEndDistance",
        "edgeFadeStart",
        "edgeFadeWidth",
    };
    public static readonly int[] FieldLengths =
    {
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
    };

    public float size { get; set; }
    public float waveLength { get; set; }
    public float waveSteepness { get; set; }
    public float normalUvScale { get; set; }
    public float wavePatternUvScale { get; set; }
    public float fogStartDistance { get; set; }
    public float fogEndDistance { get; set; }
    public float edgeFadeStart { get; set; }
    public float edgeFadeWidth { get; set; }

    public float[] ToFloatArray() => new float[]
    {
        size,
        waveLength,
        waveSteepness,
        normalUvScale,
        wavePatternUvScale,
        fogStartDistance,
        fogEndDistance,
        edgeFadeStart,
        edgeFadeWidth,
    };
}
