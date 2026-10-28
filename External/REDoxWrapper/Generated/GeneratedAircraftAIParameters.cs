namespace REDoxWrapper.Generated;

public sealed class GeneratedAircraftAIParameters
{
    public const int FieldCount = 10;
    public static readonly string[] FieldNames =
    {
        "directionChangeSeconds",
        "altitudeChangeSeconds",
        "lowAltitude",
        "highAltitude",
        "pitchLookAheadDistance",
        "turnGain",
        "pitchGain",
        "speedMatchGain",
        "highGManeuverIntervalSeconds",
        "highGManeuverDurationSeconds",
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
        1,
    };

    public float directionChangeSeconds { get; set; }
    public float altitudeChangeSeconds { get; set; }
    public float lowAltitude { get; set; }
    public float highAltitude { get; set; }
    public float pitchLookAheadDistance { get; set; }
    public float turnGain { get; set; }
    public float pitchGain { get; set; }
    public float speedMatchGain { get; set; }
    public float highGManeuverIntervalSeconds { get; set; }
    public float highGManeuverDurationSeconds { get; set; }

    public float[] ToFloatArray() => new float[]
    {
        directionChangeSeconds,
        altitudeChangeSeconds,
        lowAltitude,
        highAltitude,
        pitchLookAheadDistance,
        turnGain,
        pitchGain,
        speedMatchGain,
        highGManeuverIntervalSeconds,
        highGManeuverDurationSeconds,
    };
}
