# Actual scene-settings loader comparison

Runs the production `FlightSceneLoader::Load` and the settings-only portion of
`CombatAudio::Initialize`, using the current Assets. The game itself is not changed.

```powershell
./Tools/SceneSettingsBenchmark/Run.ps1 -ProcessRuns 5
```

Requires the existing pinned nlohmann/json 3.12.0 header at
`x64/JsonParserFairBenchmark/deps/json.hpp`, Python, MSVC x64 and .NET 10 Native AOT.
The script verifies the header's SHA-256 and publishes the current production
RE:DOX wrapper in Release mode. Generated native layouts come from the production
C# bindings and the retro-film reader. Build artifacts stay in x64.

## What is compared

- RE:DOX: the actual production Native AOT DLL, including its normal handle
  management, locks, value cache, UTF-8 conversion and data validation.
- nlohmann/json: a benchmark-only native adapter implementing the APIs consumed
  by this loader. It owns parsed documents, supports numeric/string/JSON Pointer
  lookups, checks finite numeric values and array lengths, and uses the same
  one-pass placement and document-scoped cache for typed numeric settings.
- Both execute the same compiled game loader, filesystem reads/checks, optional
  field handling, configuration validation, cache lifetimes, and destruction.
  Audio settings use the production document-scoped cache, including its cleanup.
  No DLL load is artificially added to the native backend.

Backend selection is isolated to the benchmark's forced-include header; the
production Engine and Game sources are not edited. During timed RE:DOX runs,
function resolution returns the original production export directly, avoiding
extra tracing wrappers. Native functions remain behind the benchmark adapter;
this is a comparison of these usable implementations, not a universal ranking
of the parser libraries or an optimized native rewrite of the game.

## Measurements and correctness

1. First complete settings load in a fresh process, including RE:DOX DLL load and
   runtime initialization, native layout initialization, reads, lookups and cleanup.
   This does **not** flush the Windows file cache or measure process launch time.
2. A separate untimed validation run logs every consumed API result (float bits,
   strings, array lengths, missing fields, bulk results) plus the JSON input bytes.
   Paired traces must have identical SHA-256 hashes; otherwise the runner fails.
3. Five warm-up loads, then seven approximately 150ms samples per process.
   Each iteration creates fresh loader/document/value caches; this measures
   repeated complete loading, **not** just copying a prefilled cache.
4. Backends run sequentially, alternating order for each process pair. A five-pair
   run produces five first-load observations and 35 warm samples per backend.

The trace validates all API results used to construct the scene; it does not
compare padding bytes or unrelated asset contents. The native adapter is for
these benchmark inputs, not a production replacement with complete malformed-
input, duplicate-key or concurrency compatibility. First-load and warmed
results must be reported separately. Avoid claiming a game-wide FPS or total
startup improvement: GPU initialization, model/texture loading, sound decoding,
window creation, and rendering are excluded.

Outputs: per-process CSVs and first-load info, paired validation traces,
`summary.csv` (warm sample medians), environment details, and input hashes.

Before timing, the runner also builds/runs `CacheChecks.cpp` and compiles the
production audio implementation. Checks cover absolute-path cache hits, normalized
aliases, map insertion stability, invalidation by cache lifetime, relative paths
after working-directory changes, move construction/assignment, audio defaults
and contextual validation errors. The moved-from cache remains usable.

Historical pre-optimization results used uncached audio settings and repeated
path normalization. Keep their archived outputs separate when comparing changes.
