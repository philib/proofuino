#include <Arduino.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include "Temperatures.h"

OneWire oneWire;
DallasTemperature sensors(&oneWire);

class TemperatureManager
{
public:
    enum State
    {
        ON,
        OFF
    };

private:
    int pin;

public:
    TemperatureManager(int pin) : pin(pin)
    {
        oneWire.begin(pin);
        sensors.begin();
    }
    Temperatures getTemperatures(std::function<void()> onError)
    {
        float dough = 0.0;
        float box = 0.0;

        sensors.requestTemperatures();

        float tempTAC = sensors.getTempCByIndex(0);
        float tempTDC = sensors.getTempCByIndex(1);
        if (tempTAC != DEVICE_DISCONNECTED_C && tempTDC != DEVICE_DISCONNECTED_C)
        {
            dough = tempTDC;
            box = tempTAC;
        }
        else
        {
            onError();
        }
        return Temperatures(box, dough);
    }
};