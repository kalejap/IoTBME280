## Purpose

Controls whether the sea-level pressure and dew point values are exposed as Home Assistant sensor entities; by default they are computed internally but not registered or published.

## ADDED Requirements

### Requirement: Derived sensors are disabled by default
The sea-level pressure and dew point HA entities SHALL NOT be registered with the MQTT broker or included in published payloads unless the caller has explicitly enabled them before `begin()` is called.

#### Scenario: Default instantiation omits derived entities
- **WHEN** `IoTBME280Sensor` is constructed without calling `enableSeaLevelPressure(uid)` or `enableDewPoint(uid)`
- **THEN** no sea-level pressure or dew point HA sensor entity is announced or published to MQTT

### Requirement: Sea-level pressure entity is enabled on demand
The class SHALL expose an `enableSeaLevelPressure(const char* uid)` method that accepts the HA entity ID and, when called before `begin()`, causes the sea-level pressure HA entity to be registered and published on each `publishValue()` call.

#### Scenario: Sea-level entity enabled before setup
- **WHEN** `enableSeaLevelPressure(uid)` is called with a non-null UID before `begin()`
- **THEN** the sea-level pressure HA entity is announced under that UID and its value is published on each `publishValue()` call

#### Scenario: Enable call after begin has no effect
- **WHEN** `enableSeaLevelPressure(uid)` is called after `begin()` has already executed
- **THEN** the sea-level pressure HA entity is NOT retroactively registered or published

### Requirement: Dew point entity is enabled on demand
The class SHALL expose an `enableDewPoint(const char* uid)` method that accepts the HA entity ID and, when called before `begin()`, causes the dew point HA entity to be registered and published on each `publishValue()` call.

#### Scenario: Dew point entity enabled before setup
- **WHEN** `enableDewPoint(uid)` is called with a non-null UID before `begin()`
- **THEN** the dew point HA entity is announced under that UID and its value is published on each `publishValue()` call

#### Scenario: Enable call after begin has no effect
- **WHEN** `enableDewPoint(uid)` is called after `begin()` has already executed
- **THEN** the dew point HA entity is NOT retroactively registered or published

### Requirement: Raw derived values remain accessible regardless of entity state
The methods `getSeaLevelPressureHPa()` and `getDewPointC()` SHALL return the most recently computed value whether or not the corresponding HA entities are enabled.

#### Scenario: Derived value readable without entity
- **WHEN** `enableSeaLevelPressure(uid)` has not been called but `update()` has run
- **THEN** `getSeaLevelPressureHPa()` returns a valid computed value

#### Scenario: Derived value readable with entity enabled
- **WHEN** `enableSeaLevelPressure(uid)` has been called and `update()` has run
- **THEN** `getSeaLevelPressureHPa()` returns the same value that was published

### Requirement: Named entity accessors reflect enabled state
The `seaLevelPressure()` and `dewPoint()` named accessor methods SHALL only be valid after the corresponding `enable*()` method has been called.

#### Scenario: Accessor returns reference when enabled
- **WHEN** `enableSeaLevelPressure(uid)` was called and the sensor has been set up
- **THEN** `seaLevelPressure()` returns a valid `IoTHASensorNumberWrapper<float>` reference

#### Scenario: Accessor is not called when entity is disabled
- **WHEN** `enableSeaLevelPressure(uid)` was not called
- **THEN** calling `seaLevelPressure()` results in undefined behaviour (callers MUST guard with `isSeaLevelPressureEnabled()`)

### Requirement: Query methods report enabled state
The class SHALL provide `isSeaLevelPressureEnabled()` and `isDewPointEnabled()` boolean methods so callers can guard accessor use at runtime.

#### Scenario: Enabled flag reflects prior enable call
- **WHEN** `enableSeaLevelPressure(uid)` was called
- **THEN** `isSeaLevelPressureEnabled()` returns `true`

#### Scenario: Disabled flag when no enable call made
- **WHEN** `enableSeaLevelPressure(uid)` was not called
- **THEN** `isSeaLevelPressureEnabled()` returns `false`
