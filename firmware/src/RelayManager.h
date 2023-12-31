#include <Arduino.h>

class RelayManager
{
public:
    enum State
    {
        ON,
        OFF
    };

private:
    int pin;
    bool active;

public:
    RelayManager(int pin) : pin(pin), active(true)
    {
        pinMode(pin, OUTPUT);
    }
    void turnOn()
    {
        if (active)
        {
            digitalWrite(pin, HIGH);
        }
    }
    void turnOff()
    {
        digitalWrite(pin, LOW);
    }
    void disable()
    {
        active = false;
        turnOff();
    }
    void enable()
    {
        active = true;
    }
    State getRelayState()
    {
        return digitalRead(pin) == HIGH ? ON : OFF;
    }
};