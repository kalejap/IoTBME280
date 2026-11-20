## Why

`IoTBME280Sensor` currently hard-wires all five HA entities on every instantiation, including sea-level pressure and dew point, whose derived values are only meaningful when altitude is known and the application actually wants them. Users who need only temperature, humidity, and raw pressure must still register, configure, and publish the two derived entities. Adding an `setAltitude()` setter removes the need to know altitude at construction time, which matters when altitude is read from stored geo-location settings after the object is created.

## What Changes

- Sea-level pressure and dew point HA entities become **opt-in**: they are not registered or published unless the caller explicitly enables them before `begin()`.
- Two `enable*()` methods — `enableSeaLevelPressure(const char* uid)` and `enableDewPoint(const char* uid)` — accept the HA entity UID and allow the derived IoTDevice subclass constructor to activate each entity individually; the UIDs are no longer part of the `IoTBME280Sensor` constructor. **BREAKING**: `seaLevelUid` and `dewPointUid` constructor parameters are removed.
- `setAltitude(float altitudeM)` added as a public setter so altitude can be supplied or updated at any point before or after construction (e.g. from stored geo settings in `postSetup()`).
- Raw computation of sea-level pressure and dew point continues unconditionally in `update()` so the values remain accessible via `getSeaLevelPressureHPa()` / `getDewPointC()` regardless of whether the HA entities are enabled.

## Capabilities

### New Capabilities

- `optional-derived-sensors`: Sea-level pressure and dew point become opt-in HA entities; `enableSeaLevelPressure(uid)` and `enableDewPoint(uid)` accept the HA entity UID and control whether each entity is registered and published.
- `set-altitude`: `setAltitude(float)` setter that updates the altitude used for sea-level pressure computation at any point in the object's lifetime.

### Modified Capabilities

_(none — no existing spec files exist)_

## Impact

- **`IoTBME280Sensor.h`**: Primary change site — constructor loses `seaLevelUid` and `dewPointUid` parameters; base type narrows from `NCompositeWrapper<…, 5>` to `NCompositeWrapper<…, 3>`; `enableSeaLevelPressure(uid)`, `enableDewPoint(uid)`, and `setAltitude()` added to public interface.
- **Callers** (`ThermostatDevice.cpp` and any other IoTDevice subclass): **BREAKING** — must remove `seaLevelUid` / `dewPointUid` from the constructor call and instead call `m_bme.enableSeaLevelPressure("bme_sea")` / `m_bme.enableDewPoint("bme_dew")` in the IoTDevice subclass constructor to restore previous behaviour.
- **No new external dependencies.**
