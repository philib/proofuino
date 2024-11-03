#ifndef TEMPERATURES_H
#define TEMPERATURES_H

class Temperature
{
private:
    float value;

public:
    Temperature(float value) : value(value) {}
    float getValue()
    {
        return value;
    }
    bool isBelow(float other)
    {
        return value < other;
    }
    bool isAbove(float other)
    {
        return value > other;
    }
};

class Temperatures
{
public:
    Temperature box;
    Temperature dough;

    Temperatures(float tac, float tdc)
        : box(Temperature(tac)), dough(Temperature(tdc)) {}
};

#endif