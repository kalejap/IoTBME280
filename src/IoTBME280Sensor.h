/*
  IoTBME280Sensor.h - IoT framework wrapper for the BME280 environmental sensor.
  Publishes temperature, humidity, and atmospheric pressure as three mandatory Home
  Assistant sensor entities over MQTT.  Sea-level pressure and dew point are optional:
  call enableSeaLevelPressure(uid) / enableDewPoint(uid) in the IoTDevice subclass
  constructor to activate them.

  Copyright (c) 2026 Peter Kaleja.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef IOTBME280SENSOR_H
#define IOTBME280SENSOR_H

#include <new>
#include <math.h>
#include <Adafruit_BME280.h>
#include <IoTHACompositeDeviceWrapper.h>
#include <IoTHASensorNumberWrapper.h>
#include <AppSettings.h>
#include <IoTDebug.h>
#include <JSONUtils.h>
#include "BME280LanguageSupport.h"

/**
 * @class IoTBME280Sensor
 * @brief IoT framework wrapper for one BME280 chip.
 *
 * Mandatory HA entities (always present):
 *   0 — temperature       (°C or °F)
 *   1 — humidity          (%)
 *   2 — pressure          (hPa)
 *
 * Optional HA entities (opt-in before begin()):
 *   3 — sea-level pressure (hPa)   — call enableSeaLevelPressure(uid)
 *   4 — dew point          (°C/°F) — call enableDewPoint(uid)
 *
 * Raw derived values (getSeaLevelPressureHPa(), getDewPointC()) are always
 * computed by update() regardless of whether the HA entities are enabled.
 *
 * Usage pattern:
 *   1. Construct with 3 mandatory HA entity UIDs, altitude in metres (default 0),
 *      and optional I²C address (default 0x76).
 *   2. In the IoTDevice subclass constructor, call enableSeaLevelPressure(uid)
 *      and/or enableDewPoint(uid) to register the optional entities.
 *   3. Optionally call setUseFahrenheit(true) and/or setAltitude() before begin().
 *   4. Register with IoTDevice::registerComponent() — begin() is called by preSetup().
 *   5. Configure HA entity names/device-class via named accessors in postSetup().
 *   6. In the loop (via a Timer): call update() then publishValue().
 *
 * Example:
 * @code
 *   // Member declaration:
 *   IoTBME280Sensor m_bme;
 *
 *   // Constructor body:
 *   MyDevice::MyDevice(const DeviceProperties& props)
 *       : IoTDevice(props)
 *       , m_bme("bme_temp", "bme_hum", "bme_press", 150.0f)
 *   {
 *       m_bme.enableSeaLevelPressure("bme_sea");
 *       m_bme.enableDewPoint("bme_dew");
 *       registerComponent(m_bme);
 *   }
 *
 *   // In postSetup():
 *   m_bme.temperature().setName("Room temperature");
 *   m_bme.humidity().setName("Room humidity");
 *   m_bme.pressure().setName("Pressure");
 *   if (m_bme.isSeaLevelPressureEnabled())
 *       m_bme.seaLevelPressure().setName("Sea-level pressure");
 *   if (m_bme.isDewPointEnabled())
 *       m_bme.dewPoint().setName("Dew point");
 *
 *   // In postLoop() via a Timer:
 *   m_bme.update();
 *   m_bme.publishValue();
 * @endcode
 */
class IoTBME280Sensor : public IoTHACompositeDeviceWrapperBase
{
public:
    /**
     * @brief Construct the sensor and the three mandatory HA entity wrappers.
     *
     * @param tempUid       HA entity ID for temperature.
     * @param humUid        HA entity ID for humidity.
     * @param pressUid      HA entity ID for atmospheric pressure.
     * @param altitudeM     Local altitude in metres above sea level (default 0).
     * @param i2cAddress    BME280 I²C address (default 0x76; use 0x77 if SDO is high).
     */
    IoTBME280Sensor(const char* tempUid,
                    const char* humUid,
                    const char* pressUid,
                    float       altitudeM  = 0.0f,
                    uint8_t     i2cAddress = 0x76)
        : _tempWrapper(tempUid,  HABaseDeviceType::PrecisionP1)
        , _humWrapper(humUid,    HABaseDeviceType::PrecisionP1)
        , _pressWrapper(pressUid, HABaseDeviceType::PrecisionP0)
        , _altitudeM(altitudeM)
        , _i2cAddress(i2cAddress)
    {}

    IoTBME280Sensor(const IoTBME280Sensor&)            = delete;
    IoTBME280Sensor& operator=(const IoTBME280Sensor&) = delete;
    IoTBME280Sensor(IoTBME280Sensor&&)                 = delete;
    IoTBME280Sensor& operator=(IoTBME280Sensor&&)      = delete;

    // -----------------------------------------------------------------------
    // Named accessors — mandatory entities
    // -----------------------------------------------------------------------

    /** @brief HA entity for temperature (slot 0). */
    IoTHASensorNumberWrapper<float>& temperature()       { return _tempWrapper; }
    const IoTHASensorNumberWrapper<float>& temperature() const { return _tempWrapper; }

    /** @brief HA entity for relative humidity (slot 1). */
    IoTHASensorNumberWrapper<float>& humidity()          { return _humWrapper; }
    const IoTHASensorNumberWrapper<float>& humidity()    const { return _humWrapper; }

    /** @brief HA entity for atmospheric pressure (slot 2). */
    IoTHASensorNumberWrapper<float>& pressure()          { return _pressWrapper; }
    const IoTHASensorNumberWrapper<float>& pressure()    const { return _pressWrapper; }

    // -----------------------------------------------------------------------
    // Named accessors — optional entities (guard with is*Enabled())
    // -----------------------------------------------------------------------

    /**
     * @brief HA entity for sea-level pressure.
     * Precondition: isSeaLevelPressureEnabled() must return true.
     */
    IoTHASensorNumberWrapper<float>& seaLevelPressure()
    {
        return *reinterpret_cast<IoTHASensorNumberWrapper<float>*>(_seaLevelBuf);
    }
    const IoTHASensorNumberWrapper<float>& seaLevelPressure() const
    {
        return *reinterpret_cast<const IoTHASensorNumberWrapper<float>*>(_seaLevelBuf);
    }

    /**
     * @brief HA entity for dew point.
     * Precondition: isDewPointEnabled() must return true.
     */
    IoTHASensorNumberWrapper<float>& dewPoint()
    {
        return *reinterpret_cast<IoTHASensorNumberWrapper<float>*>(_dewPointBuf);
    }
    const IoTHASensorNumberWrapper<float>& dewPoint() const
    {
        return *reinterpret_cast<const IoTHASensorNumberWrapper<float>*>(_dewPointBuf);
    }

    // -----------------------------------------------------------------------
    // Raw value accessors — always in SI units regardless of Fahrenheit flag
    // -----------------------------------------------------------------------

    /** @brief Last read temperature in °C, or NaN when sensor absent. */
    float getTemperatureC()        const { return _rawTempC; }

    /** @brief Last read relative humidity in %, or NaN when sensor absent. */
    float getHumidityPct()         const { return _rawHumidity; }

    /** @brief Last read atmospheric pressure in hPa, or NaN when sensor absent. */
    float getPressureHPa()         const { return _rawPressureHPa; }

    /** @brief Derived sea-level pressure in hPa, or NaN when sensor absent. */
    float getSeaLevelPressureHPa() const { return _seaLevelHPa; }

    /** @brief Derived dew point in °C, or NaN when sensor absent. */
    float getDewPointC()           const { return _dewPointC; }

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    /**
     * @brief Select Fahrenheit output for temperature and dew point entities.
     *        Must be called before begin().
     */
    void setUseFahrenheit(bool useFahrenheit) { _useFahrenheit = useFahrenheit; }

    /** @brief Returns true if begin() found the BME280 on the I²C bus. */
    bool sensorFound() const { return _sensorFound; }

    /**
     * @brief Update the altitude used for sea-level pressure computation.
     *        Takes effect on the next update() call.
     *
     * @param altitudeM  Local altitude in metres (negative values accepted).
     */
    void setAltitude(float altitudeM)
    {
        _altitudeM = altitudeM;
        IOTLOGDEBUG1(F("BME280: altitude set to"), _altitudeM);
    }

    // -----------------------------------------------------------------------
    // Optional entity activation — call in IoTDevice subclass constructor,
    // before registerComponent(), never after begin().
    // -----------------------------------------------------------------------

    /**
     * @brief Activate the sea-level pressure HA entity.
     *
     * Constructs the HA wrapper and schedules it for MQTT registration.
     * Has no effect if called after begin().
     *
     * @param uid  Unique Home Assistant entity ID for the sea-level pressure sensor.
     */
    void enableSeaLevelPressure(const char* uid)
    {
        if (_seaLevelEnabled)
        {
            return;
        }
        if (_begun)
        {
            IOTLOGWARN(F("BME280: enableSeaLevelPressure() called after begin() - ignored"));
            return;
        }
        new (_seaLevelBuf) IoTHASensorNumberWrapper<float>(uid);
        _seaLevelEnabled = true;
    }

    /**
     * @brief Activate the dew point HA entity.
     *
     * Constructs the HA wrapper and schedules it for MQTT registration.
     * Has no effect if called after begin().
     *
     * @param uid  Unique Home Assistant entity ID for the dew point sensor.
     */
    void enableDewPoint(const char* uid)
    {
        if (_dewPointEnabled)
        {
            return;
        }
        if (_begun)
        {
            IOTLOGWARN(F("BME280: enableDewPoint() called after begin() - ignored"));
            return;
        }
        new (_dewPointBuf) IoTHASensorNumberWrapper<float>(uid, HABaseDeviceType::PrecisionP1);
        _dewPointEnabled = true;
    }

    /** @brief Returns true if the sea-level pressure HA entity has been enabled. */
    bool isSeaLevelPressureEnabled() const { return _seaLevelEnabled; }

    /** @brief Returns true if the dew point HA entity has been enabled. */
    bool isDewPointEnabled() const { return _dewPointEnabled; }

    // -----------------------------------------------------------------------
    // Framework child iteration — exposes optional entities to the framework
    // -----------------------------------------------------------------------

    /**
     * @brief Returns 3 plus the number of enabled optional entities (0, 1, or 2).
     */
    size_t childCount() const override
    {
        return 3u
            + (_seaLevelEnabled ? 1u : 0u)
            + (_dewPointEnabled ? 1u : 0u);
    }

    /**
     * @brief Returns a pointer to the i-th child wrapper.
     *
     * Indices 0–2 delegate to the three-slot base.
     * Index 3 maps to the sea-level wrapper (if enabled), or the dew point
     * wrapper (if only dew point is enabled).
     * Index 4 maps to the dew point wrapper when both are enabled.
     */
    IoTHADeviceWrapperBase* childAt(size_t i) const override
    {
        switch (i)
        {
            case 0: return const_cast<IoTHASensorNumberWrapper<float>*>(&_tempWrapper);
            case 1: return const_cast<IoTHASensorNumberWrapper<float>*>(&_humWrapper);
            case 2: return const_cast<IoTHASensorNumberWrapper<float>*>(&_pressWrapper);
            default: break;
        }
        size_t optIdx = i - 3u;
        if (_seaLevelEnabled)
        {
            if (optIdx == 0)
            {
                return const_cast<IoTHASensorNumberWrapper<float>*>(
                    reinterpret_cast<const IoTHASensorNumberWrapper<float>*>(_seaLevelBuf));
            }
            optIdx--;
        }
        if (_dewPointEnabled && optIdx == 0)
        {
            return const_cast<IoTHASensorNumberWrapper<float>*>(
                reinterpret_cast<const IoTHASensorNumberWrapper<float>*>(_dewPointBuf));
        }
        return nullptr;
    }

    // -----------------------------------------------------------------------
    // Runtime methods
    // -----------------------------------------------------------------------

    /**
     * @brief Read all values from the BME280 and compute derived quantities.
     *        Call periodically from a Timer in the application loop.
     * @return true on success, false if the sensor was not found during begin().
     */
    bool update(bool /*force*/ = false) override
    {
        if (!_sensorFound)
        {
            return false;
        }

        _rawTempC       = _bme.readTemperature();
        _rawHumidity    = _bme.readHumidity();
        _rawPressureHPa = _bme.readPressure() / 100.0f;
        _seaLevelHPa    = _bme.seaLevelForAltitude(_altitudeM, _rawPressureHPa);
        _dewPointC      = computeDewPoint(_rawTempC, _rawHumidity);

        IOTLOGDEBUG2(F("BME280: temp/hum"),   _rawTempC,       _rawHumidity);
        IOTLOGDEBUG2(F("BME280: press/sea"),  _rawPressureHPa, _seaLevelHPa);
        IOTLOGDEBUG1(F("BME280: dew"),        _dewPointC);

        return true;
    }

    /**
     * @brief Convert cached values to the configured unit and publish all active
     *        HA entities to MQTT.
     * @param force  Pass true to publish even if values have not changed.
     * @return true if all active entities published successfully; false if sensor absent.
     */
    bool publishValue(const bool force = false) override
    {
        if (!_sensorFound)
        {
            return false;
        }

        const bool inCelsius = !_useFahrenheit;

        temperature().setCurrentValue(AppSettings::convertTemperature(_rawTempC, inCelsius));
        humidity().setCurrentValue(_rawHumidity);
        pressure().setCurrentValue(_rawPressureHPa);

        bool ok = true;
        ok &= temperature().publishValue(force);
        ok &= humidity().publishValue(force);
        ok &= pressure().publishValue(force);

        if (_seaLevelEnabled)
        {
            seaLevelPressure().setCurrentValue(_seaLevelHPa);
            ok &= seaLevelPressure().publishValue(force);
        }

        if (_dewPointEnabled)
        {
            dewPoint().setCurrentValue(AppSettings::convertTemperature(_dewPointC, inCelsius));
            ok &= dewPoint().publishValue(force);
        }

        return ok;
    }

    /**
     * @brief Return a JSON fragment summarising all active sensor values.
     *        Returns an empty String when the sensor was not found.
     */
    String statusJSON() const override
    {
        if (!_sensorFound)
        {
            return String();
        }

        const bool  inCelsius = !_useFahrenheit;
        const char* tempUnit  = AppSettings::temperatureUnit(inCelsius);

        String result;

        auto addEntry = [&](const char* nameStr, float value, const char* unit)
        {
            if (!result.isEmpty())
            {
                result += ',';
            }
            String obj;
            obj += JSONUtils::Pair(F("name"),  nameStr, true);
            obj += JSONUtils::Pair(F("value"), value);
            obj += JSONUtils::Pair(F("unit"),  unit);
            result += JSONUtils::EncloseObject(obj);
        };

        addEntry(temperature().name() && temperature().name()[0] ? temperature().name() : L_BME280_TEMPERATURE,
                 AppSettings::convertTemperature(_rawTempC, inCelsius), tempUnit);
        addEntry(humidity().name() && humidity().name()[0] ? humidity().name() : L_BME280_HUMIDITY,
                 _rawHumidity, "%");
        addEntry(pressure().name() && pressure().name()[0] ? pressure().name() : L_BME280_PRESSURE,
                 _rawPressureHPa, "hPa");

        if (_seaLevelEnabled)
        {
            addEntry(seaLevelPressure().name() && seaLevelPressure().name()[0]
                     ? seaLevelPressure().name() : L_BME280_SEA_LEVEL,
                     _seaLevelHPa, "hPa");
        }

        if (_dewPointEnabled)
        {
            addEntry(dewPoint().name() && dewPoint().name()[0]
                     ? dewPoint().name() : L_BME280_DEW_POINT,
                     AppSettings::convertTemperature(_dewPointC, inCelsius), tempUnit);
        }

        return result;
    }

protected:
    /**
     * @brief Initialise the BME280 and configure HA entity units.
     *        Called automatically by IoTDevice::preSetup().
     */
    void begin() override
    {
        _begun = true;
        _sensorFound = _bme.begin(_i2cAddress);

        if (!_sensorFound)
        {
            IOTLOGWARN(F("BME280: sensor not found on I2C bus"));
            return;
        }

        const char* tempUnit = AppSettings::temperatureUnit(!_useFahrenheit);
        temperature().setUnitOfMeasurement(tempUnit);
        humidity().setUnitOfMeasurement("%");
        pressure().setUnitOfMeasurement("hPa");

        if (_seaLevelEnabled)
        {
            seaLevelPressure().setUnitOfMeasurement("hPa");
        }

        if (_dewPointEnabled)
        {
            dewPoint().setUnitOfMeasurement(tempUnit);
        }

        IOTLOGINFO(F("BME280: sensor initialised"));
    }

private:
    /** @brief Magnus formula dew point (accurate ±0.35 °C for 0–60 °C, 1–100 % RH). */
    static float computeDewPoint(float tempC, float humidity)
    {
        const float a = 17.625f;
        const float b = 243.04f;
        float gamma = logf(humidity / 100.0f) + (a * tempC) / (b + tempC);
        return (b * gamma) / (a - gamma);
    }

    IoTHASensorNumberWrapper<float> _tempWrapper;
    IoTHASensorNumberWrapper<float> _humWrapper;
    IoTHASensorNumberWrapper<float> _pressWrapper;

    Adafruit_BME280 _bme;
    float           _altitudeM      = 0.0f;
    uint8_t         _i2cAddress     = 0x76;
    bool            _useFahrenheit  = false;
    bool            _sensorFound    = false;
    bool            _seaLevelEnabled = false;
    bool            _dewPointEnabled = false;
    bool            _begun           = false;

    alignas(IoTHASensorNumberWrapper<float>) char _seaLevelBuf[sizeof(IoTHASensorNumberWrapper<float>)];
    alignas(IoTHASensorNumberWrapper<float>) char _dewPointBuf[sizeof(IoTHASensorNumberWrapper<float>)];

    float _rawTempC       = NAN;
    float _rawHumidity    = NAN;
    float _rawPressureHPa = NAN;
    float _seaLevelHPa    = NAN;
    float _dewPointC      = NAN;
};

#endif // IOTBME280SENSOR_H
