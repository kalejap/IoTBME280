## Context

`IoTBME280Sensor` currently inherits from `NCompositeWrapper<IoTHASensorNumberWrapper<float>, 5>`, constructing all five HA entity wrappers unconditionally. Altitude is a constructor parameter. See `proposal.md` for motivation.

## Goals / Non-Goals

**Goals:**
- Constructor takes only three UIDs (temperature, humidity, raw pressure).
- Sea-level pressure and dew point wrappers exist and are registered only when `enableSeaLevelPressure(uid)` / `enableDewPoint(uid)` are called before `begin()`.
- `setAltitude()` updates the altitude used for sea-level computation at any point in the object's lifetime.

**Non-Goals:**
- Changing the number of HA entities at runtime after `begin()` has executed.
- Dynamic/heap allocation of wrapper objects.

## Decisions

### Decision 1: Narrow to three-slot base; store optional wrappers in aligned member buffers

**Chosen**: Change the base from `NCompositeWrapper<IoTHASensorNumberWrapper<float>, 5>` to `NCompositeWrapper<IoTHASensorNumberWrapper<float>, 3>` (temperature, humidity, raw pressure). The two optional wrappers are held in `alignas` / `sizeof` char buffers inside `IoTBME280Sensor` and constructed in-place via placement-new inside `begin()` only when their `enable*()` has been called beforehand. The UID passed to `enable*()` is stored as a `const char*` member until `begin()` runs.

**Rationale**: The optional wrappers physically don't exist until enabled — no phantom objects are announced to MQTT. Memory footprint equals the previous 5-slot struct (char buffers occupy the same space) but no object is constructed unless needed.

**Alternative considered — keep 5-slot base and guard with bool flags**: Simpler diff, no placement-new, but retains phantom constructor UIDs and always constructs both wrapper objects. Rejected because the goal is to remove those constructor parameters entirely.

**Alternative considered — heap allocation (`new`)**: Not appropriate for ESP8266 firmware; heap fragmentation is a real concern in long-running embedded code.

---

### Decision 2: `begin()` is the construction and enforcement point

The UID pointers stored by `enable*()` are used inside `begin()` to construct the optional wrappers in-place and call `setUnitOfMeasurement()`. Calls to `enable*()` after `begin()` are logged with `IOTLOGWARN` and ignored — no retroactive registration.

**Rationale**: `begin()` is the single point where HA discovery is triggered. Constructing wrappers there keeps object lifetime predictable and avoids guard checks on every downstream call.

---

### Decision 4: Override `childCount()` and `childAt()` to expose optional wrappers to the framework

**Chosen**: Override `childCount()` to return `BaseType::childCount()` (3) plus the number of enabled optional wrappers. Override `childAt(index)` to map indices 3 and 4 to the placement-new wrapper objects when they are enabled, delegating lower indices to the base.

**Rationale**: The IoT framework iterates children via `childCount()` / `childAt()` for lifecycle calls (MQTT discovery, `begin()`, periodic updates). Without the override, the optional wrappers are invisible to the framework and would need every framework entry-point in `IoTBME280Sensor` to contain explicit special-case code for each optional entity. The override makes the optional entities first-class framework citizens and keeps `begin()` / `publishValue()` / `statusJSON()` clean.

**Implication for `begin()`**: The `begin()` override constructs the optional wrappers via placement-new (Decision 1) and calls `setUnitOfMeasurement()` on them. The framework then reaches them through `childAt()` for subsequent lifecycle calls. No duplicate iteration is needed.

---

### Decision 3: `setAltitude()` is a plain setter with no side effects

`setAltitude(float altitudeM)` stores the value in `_altitudeM`. The new altitude takes effect on the next `update()` call.

**Rationale**: `update()` already reads `_altitudeM` on each invocation, so no additional mechanism is needed.

## Risks / Trade-offs

- **Silent misuse of disabled accessors** → `seaLevelPressure()` / `dewPoint()` dereference placement-new storage that may be uninitialised if `enable*()` was never called. Mitigated by: (a) `is*Enabled()` query methods, (b) documentation that callers must guard.
- **Post-begin `enable*()` silently no-ops** → `IOTLOGWARN` is emitted so a developer who calls `enable*()` in `postSetup()` (too late) sees the warning in serial output.
- **Placement-new requires explicit destructor call** → The char buffers are never destroyed (sensor lives for the duration of the application), so no destructor management is required in practice.

## Migration Plan

For the initial implementation (new library, no existing callers to migrate):
1. Update `IoTBME280Sensor` constructor to 3 UIDs.
2. Update all call sites to remove `seaLevelUid` / `dewPointUid` from the constructor call and add `enableSeaLevelPressure(uid)` / `enableDewPoint(uid)` in the IoTDevice subclass constructor where the entities are wanted.

Rollback: revert `IoTBME280Sensor.h` — no stored data, schema, or protocol changes are involved.
