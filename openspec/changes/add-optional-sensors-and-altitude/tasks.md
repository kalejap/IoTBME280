<!-- Design refinement (apply phase):
     Tasks 1.4 and 4.2/4.3 were merged into the enable*() implementation.
     Wrappers are constructed via placement-new inside enableSeaLevelPressure() /
     enableDewPoint() (not in begin()), because registerWithMqtt() iterates
     childAt() BEFORE begin() is called — constructing in begin() would cause
     virtual dispatch on uninitialized storage.  begin() now only calls
     setUnitOfMeasurement() on already-constructed wrappers.  The _seaLevelUid /
     _dewPointUid member fields (task 1.4) were consequently not needed. -->

## 1. Restructure IoTBME280Sensor base and constructor

- [x] 1.1 Change the base from `NCompositeWrapper<IoTHASensorNumberWrapper<float>, 5>` to `NCompositeWrapper<IoTHASensorNumberWrapper<float>, 3>`
- [x] 1.2 Remove `seaLevelUid` and `dewPointUid` parameters from the constructor; update the `BaseType(...)` initialiser list to pass only the three mandatory UIDs
- [x] 1.3 Add two aligned storage buffers as private members: `alignas(IoTHASensorNumberWrapper<float>) char _seaLevelBuf[sizeof(IoTHASensorNumberWrapper<float>)]` and equivalent `_dewPointBuf`
- [x] 1.4 ~~Add `const char* _seaLevelUid`/`_dewPointUid` members~~ — not needed; wrappers are constructed immediately in `enable*()` (see refinement note above)
- [x] 1.5 Add `bool _seaLevelEnabled = false`, `bool _dewPointEnabled = false`, and `bool _begun = false` private member flags

## 2. Implement enable methods and query API

- [x] 2.1 Implement `enableSeaLevelPressure(const char* uid)`: placement-new into `_seaLevelBuf`, set `_seaLevelEnabled = true`; emit `IOTLOGWARN` and return early if `_begun` is already true
- [x] 2.2 Implement `enableDewPoint(const char* uid)`: placement-new into `_dewPointBuf`, set `_dewPointEnabled = true`; emit `IOTLOGWARN` and return early if `_begun` is already true
- [x] 2.3 Implement `isSeaLevelPressureEnabled() const` returning `_seaLevelEnabled`
- [x] 2.4 Implement `isDewPointEnabled() const` returning `_dewPointEnabled`

## 3. Implement setAltitude()

- [x] 3.1 Implement `setAltitude(float altitudeM)`: update `_altitudeM`; emit `IOTLOGDEBUG` with the new value

## 4. Update begin() to configure optional wrappers

- [x] 4.1 Set `_begun = true` at the start of `begin()` (before the sensor-found early return)
- [x] 4.2 After sensor init succeeds: if `_seaLevelEnabled`, call `setUnitOfMeasurement("hPa")` on the already-constructed sea-level wrapper
- [x] 4.3 If `_dewPointEnabled`, call `setUnitOfMeasurement(tempUnit)` on the already-constructed dew point wrapper

## 5. Override childCount() and childAt()

- [x] 5.1 Override `childCount()` to return `BaseType::childCount()` plus the number of enabled optional wrappers (`_seaLevelEnabled ? 1 : 0` + `_dewPointEnabled ? 1 : 0`)
- [x] 5.2 Override `childAt(index)` to delegate indices `< BaseType::childCount()` to the base; map the next index to the sea-level wrapper (when enabled), and the following index to the dew point wrapper (when enabled); return `nullptr` for out-of-range indices

## 6. Update named accessors for optional wrappers

- [x] 6.1 Remove the existing `seaLevelPressure()` and `dewPoint()` named accessor methods (they referenced slots 3 and 4 of the old 5-slot base)
- [x] 6.2 Add new `seaLevelPressure()` accessor returning a reference into the `_seaLevelBuf` placement-new object (precondition: `_seaLevelEnabled`)
- [x] 6.3 Add new `dewPoint()` accessor returning a reference into the `_dewPointBuf` placement-new object (precondition: `_dewPointEnabled`)

## 7. Guard publishValue() and statusJSON()

- [x] 7.1 In `publishValue()`: if `_seaLevelEnabled`, call `setCurrentValue` + `publishValue` on the sea-level wrapper; otherwise skip
- [x] 7.2 In `publishValue()`: if `_dewPointEnabled`, call `setCurrentValue` + `publishValue` on the dew point wrapper; otherwise skip
- [x] 7.3 In `statusJSON()`: wrap the sea-level `addEntry` call with `if (_seaLevelEnabled)`
- [x] 7.4 In `statusJSON()`: wrap the dew point `addEntry` call with `if (_dewPointEnabled)`

## 8. Update documentation in IoTBME280Sensor.h

- [x] 8.1 Update the class-level Doxygen slot layout comment: mark slots 3 and 4 as optional and document the `enable*(uid)` / `is*Enabled()` / `setAltitude()` / `childCount()` / `childAt()` methods
- [x] 8.2 Update the usage example to show `enableSeaLevelPressure(uid)` and `enableDewPoint(uid)` called in the IoTDevice subclass constructor
- [x] 8.3 Add Doxygen `@brief` to each new public method

## 9. Update callers and build verification

- [x] 9.1 In `ThermostatDevice` constructor: remove `"bme_sea"` and `"bme_dew"` from the `m_bme(...)` constructor call; add `m_bme.enableSeaLevelPressure("bme_sea")` and `m_bme.enableDewPoint("bme_dew")`
- [x] 9.2 Build the `IoTApplicationDemo` project; confirm zero errors and zero new warnings
- [ ] 9.3 Verify that removing the `enable*()` calls from the constructor results in only three entities being published (log inspection)
