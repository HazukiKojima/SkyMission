namespace REDoxWrapper.Generated;

public sealed class GeneratedAircraftAIParameters
{
    public const int FieldCount = 25;
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
        "reactionSeconds",
        "maneuverSeconds",
        "disengageDistance",
        "threatDistance",
        "attackDistance",
        "aimAngleDegrees",
        "aimHoldSeconds",
        "openingGraceSeconds",
        "aimErrorDegrees",
        "cruiseSpeed",
        "attackSpeed",
        "escapeSpeed",
        "reengageDistance",
        "attackRunSeconds",
        "separationLookAheadSeconds",
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
    public float reactionSeconds { get; set; }
    public float maneuverSeconds { get; set; }
    public float disengageDistance { get; set; }
    public float threatDistance { get; set; }
    public float attackDistance { get; set; }
    public float aimAngleDegrees { get; set; }
    public float aimHoldSeconds { get; set; }
    public float openingGraceSeconds { get; set; }
    public float aimErrorDegrees { get; set; }
    public float cruiseSpeed { get; set; }
    public float attackSpeed { get; set; }
    public float escapeSpeed { get; set; }
    public float reengageDistance { get; set; }
    public float attackRunSeconds { get; set; }
    public float separationLookAheadSeconds { get; set; }

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
        reactionSeconds,
        maneuverSeconds,
        disengageDistance,
        threatDistance,
        attackDistance,
        aimAngleDegrees,
        aimHoldSeconds,
        openingGraceSeconds,
        aimErrorDegrees,
        cruiseSpeed,
        attackSpeed,
        escapeSpeed,
        reengageDistance,
        attackRunSeconds,
        separationLookAheadSeconds,
    };
}
