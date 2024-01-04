#ifndef TEMPERATURES_H
#define TEMPERATURES_H

class Range
{
public:
    float min;
    float max;
    Range(float min, float max) : min(min), max(max) {}
};

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
    bool isWithin(Range range)
    {
        return value >= range.min && value <= range.max;
    }
    bool isBelow(float other)
    {
        return value < other;
    }
    bool isBelow(Range range)
    {
        return value < range.min;
    }
    bool isAbove(float other)
    {
        return value > other;
    }
    bool isAbove(Range range)
    {
        return value > range.max;
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