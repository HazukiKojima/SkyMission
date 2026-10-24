namespace REDoxWrapper.Generated;

public sealed class GeneratedSkyParameters
{
    public const int FieldCount = 12;
    public static readonly string[] FieldNames =
    {
        "radius",
        "sunDirection",
        "sunIntensity",
        "sunColor",
        "ambientIntensity",
        "ambientColor",
    };
    public static readonly int[] FieldLengths =
    {
        1,
        3,
        1,
        3,
        1,
        3,
    };

    public float radius { get; set; }
    public float[] sunDirection { get; set; } = new float[3];
    public float sunIntensity { get; set; }
    public float[] sunColor { get; set; } = new float[3];
    public float ambientIntensity { get; set; }
    public float[] ambientColor { get; set; } = new float[3];

    public float[] ToFloatArray() => new float[]
    {
        radius,
        sunDirection[0],
        sunDirection[1],
        sunDirection[2],
        sunIntensity,
        sunColor[0],
        sunColor[1],
        sunColor[2],
        ambientIntensity,
        ambientColor[0],
        ambientColor[1],
        ambientColor[2],
    };
}
