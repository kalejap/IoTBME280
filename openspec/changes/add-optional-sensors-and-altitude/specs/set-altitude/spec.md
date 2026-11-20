## Purpose

Allows the altitude used for sea-level pressure computation to be supplied or updated at any point during the sensor's lifetime, decoupling it from the constructor call.

## ADDED Requirements

### Requirement: Altitude can be set after construction
The class SHALL expose a `setAltitude(float altitudeM)` method that updates the altitude value used for sea-level pressure computation.

#### Scenario: Altitude updated before first update() call
- **WHEN** `setAltitude(altitudeM)` is called before the first `update()` call
- **THEN** `update()` uses the new altitude to compute sea-level pressure

#### Scenario: Altitude updated between update() calls
- **WHEN** `setAltitude(altitudeM)` is called between two `update()` calls
- **THEN** the subsequent `update()` uses the new altitude value

#### Scenario: Altitude setter accepts negative values
- **WHEN** `setAltitude(-50.0f)` is called (location below sea level)
- **THEN** the value is accepted and used in sea-level pressure computation without error

### Requirement: Constructor altitude parameter remains valid
The constructor altitude parameter SHALL continue to set the initial altitude, so existing callers that pass altitude at construction time are unaffected.

#### Scenario: Constructor-supplied altitude used when no setter called
- **WHEN** `IoTBME280Sensor` is constructed with a non-zero altitude and `setAltitude()` is never called
- **THEN** `update()` uses the constructor-supplied altitude

### Requirement: Default altitude is zero
When `IoTBME280Sensor` is constructed without an explicit altitude argument and `setAltitude()` is never called, the altitude SHALL default to `0.0f` metres.

#### Scenario: Sea-level pressure equals raw pressure at zero altitude
- **WHEN** altitude is `0.0f` and `update()` runs
- **THEN** `getSeaLevelPressureHPa()` returns a value equal to `getPressureHPa()` (within floating-point precision)
