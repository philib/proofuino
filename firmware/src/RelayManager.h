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

public:
    RelayManager(int pin) : pin(pin)
    {
        pinMode(pin, OUTPUT);
    }
    void turnOn()
    {
        digitalWrite(pin, HIGH);
    }
    void turnOff()
    {
        digitalWrite(pin, LOW);
    }
    State getRelayState()
    {
        return digitalRead(pin) == HIGH ? ON : OFF;
    }
};