namespace REDoxWrapper.Generated;

public sealed class GeneratedAircraftFlightParameters
{
    public const int FieldCount = 24;
    public static readonly string[] FieldNames =
    {
        "initialSpeed",
        "minimumSpeed",
        "maximumSpeed",
        "acceleration",
        "drag",
        "maximumBankRadians",
        "pitchRateRadians",
        "turnRateRadians",
        "yawRateRadians",
        "rollLevelRate",
        "controlResponse",
        "rollInputResponse",
        "minimumTurnFactor",
        "maximumTurnFactor",
        "highGTurnMultiplier",
        "highGDrag",
        "gravity",
        "stallSpeed",
        "liftReferenceSpeed",
        "maximumLiftFactor",
        "stallAngleRadians",
        "criticalAngleRadians",
        "turnDrag",
        "flightPathResponse",
    };

    public float initialSpeed { get; set; }
    public float minimumSpeed { get; set; }
    public float maximumSpeed { get; set; }
    public float acceleration { get; set; }
    public float drag { get; set; }
    public float maximumBankRadians { get; set; }
    public float pitchRateRadians { get; set; }
    public float turnRateRadians { get; set; }
    public float yawRateRadians { get; set; }
    public float rollLevelRate { get; set; }
    public float controlResponse { get; set; }
    public float rollInputResponse { get; set; }
    public float minimumTurnFactor { get; set; }
    public float maximumTurnFactor { get; set; }
    public float highGTurnMultiplier { get; set; }
    public float highGDrag { get; set; }
    public float gravity { get; set; }
    public float stallSpeed { get; set; }
    public float liftReferenceSpeed { get; set; }
    public float maximumLiftFactor { get; set; }
    public float stallAngleRadians { get; set; }
    public float criticalAngleRadians { get; set; }
    public float turnDrag { get; set; }
    public float flightPathResponse { get; set; }

    public float[] ToFloatArray() => new float[]
    {
        initialSpeed,
        minimumSpeed,
        maximumSpeed,
        acceleration,
        drag,
        maximumBankRadians,
        pitchRateRadians,
        turnRateRadians,
        yawRateRadians,
        rollLevelRate,
        controlResponse,
        rollInputResponse,
        minimumTurnFactor,
        maximumTurnFactor,
        highGTurnMultiplier,
        highGDrag,
        gravity,
        stallSpeed,
        liftReferenceSpeed,
        maximumLiftFactor,
        stallAngleRadians,
        criticalAngleRadians,
        turnDrag,
        flightPathResponse,
    };
}
