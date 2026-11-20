# IoTBME280

`IoTBME280` is an Arduino library that wraps an Adafruit BME280 environmental
sensor for the IoT framework and publishes sensor readings as Home Assistant
entities over MQTT. It supports ESP8266 and ESP32.

The library provides temperature, relative humidity, and atmospheric pressure
entities by default. Sea-level pressure and dew point entities are optional.

## Requirements

- Arduino framework and a supported ESP8266 or ESP32 board
- BME280 sensor connected over I2C
- Adafruit BME280 and Adafruit Unified Sensor libraries
- The IoT framework providing `IoTHACompositeDeviceWrapperBase`,
  `IoTHASensorNumberWrapper`, and related classes

Install the required libraries using your project's package manager or Arduino
Library Manager. Connect the BME280 to the board's I2C bus and use its I2C
address, typically `0x76` or `0x77`.

## Usage

Create the sensor as a member of an `IoTDevice` subclass. Register the three
mandatory entities in the constructor, and enable any optional entities before
registering the component:

```cpp
#include <IoTBME280Sensor.h>

class MyDevice : public IoTDevice
{
public:
    explicit MyDevice(const DeviceProperties& props)
        : IoTDevice(props)
        , _bme("bme_temp", "bme_humidity", "bme_pressure", 150.0f, 0x76)
    {
        _bme.enableSeaLevelPressure("bme_sea_level");
        _bme.enableDewPoint("bme_dew_point");
        registerComponent(_bme);
    }

    void postSetup() override
    {
        _bme.temperature().setName("Temperature");
        _bme.humidity().setName("Humidity");
        _bme.pressure().setName("Atmospheric pressure");

        if (_bme.isSeaLevelPressureEnabled())
        {
            _bme.seaLevelPressure().setName("Sea-level pressure");
        }

        if (_bme.isDewPointEnabled())
        {
            _bme.dewPoint().setName("Dew point");
        }
    }

    void readBme280()
    {
        if (_bme.update())
        {
            _bme.publishValue();
        }
    }

private:
    IoTBME280Sensor _bme;
};
```

`registerComponent()` lets the IoT framework initialize the sensor during its
setup. Call `update()` periodically (for example, from an application timer),
then call `publishValue()` to publish the latest readings. Check `update()`'s
return value to detect an unavailable sensor.

The constructor parameters are the temperature, humidity, and pressure entity
IDs, followed by altitude in metres (default `0.0`) and the I2C address
(default `0x76`). Call `setAltitude()` to change the altitude used for
sea-level pressure calculations; the new value is used on the next `update()`.
Call `setUseFahrenheit(true)` before setup if temperature and dew-point entities
should use Fahrenheit.

Optional entities must be enabled before setup. Their `seaLevelPressure()` and
`dewPoint()` accessors should only be used when the corresponding
`isSeaLevelPressureEnabled()` or `isDewPointEnabled()` check returns `true`.
Raw values are also available through `getTemperatureC()`, `getHumidityPct()`,
`getPressureHPa()`, `getSeaLevelPressureHPa()`, and `getDewPointC()`; these
accessors use Celsius, percent, and hPa regardless of the entity temperature
unit.

## License

This library is distributed under the GNU Lesser General Public License,
version 2.1 or later. See the source files for the full license terms.
