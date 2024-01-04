#include <core_esp8266_features.h>
#include <functional>
#include <algorithm>
#include <WString.h>

enum Relay
{
    ON,
    OFF
};

enum State
{
    PAUSED,
    START,
    COOLDOWN,
    HOLD_ON,
    HOLD_OFF,
    BOOST_ON,
    BOOST_OFF,
    ERROR
};

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

class StateManager
{
private:
    const float maxDoughTemperature = 32.0f;
    const float maxBoxTemperature = 40.0f;
    unsigned long lastPhaseChange;
    float desiredDoughTemperature;
    State state;
    Relay currentRelayState;
    Temperatures currentTemperature;
    std::function<void(Relay, String)> onStateChange;
    std::function<void(String)> onError;
    void transitionTo(State newState)
    {
        if (state != newState)
        {
            lastPhaseChange = millis();
            state = newState;
            switch (state)
            {
            case HOLD_ON:
            case BOOST_ON:
                currentRelayState = ON;
                break;
            default:
                currentRelayState = OFF;
            }
            onStateChange(currentRelayState, this->getStateStringified());
        }
    }
    Range boostRange()
    {
        float upperLimit = std::min(desiredDoughTemperature + 6, maxDoughTemperature + 6);
        float lowerLimit = std::min(desiredDoughTemperature + 4, maxDoughTemperature + 4);
        return Range(lowerLimit, upperLimit);
    }
    Range holdRange()
    {
        float upperLimit = std::min(desiredDoughTemperature + 2.0f, maxDoughTemperature + 2.0f);
        float lowerLimit = std::min(desiredDoughTemperature - 0.5f, maxDoughTemperature - 0.5f);
        return Range(lowerLimit, upperLimit);
    }

public:
    StateManager(float desiredDoughTemperature, std::function<void(Relay, String)> onStateChange, std::function<void(String)> onError) : lastPhaseChange(millis()), desiredDoughTemperature(desiredDoughTemperature), state(START), currentRelayState(OFF), currentTemperature(Temperatures(0.0, 0.0)), onStateChange(onStateChange), onError(onError)
    {
        onStateChange(currentRelayState, this->getStateStringified());
    }

    void process(Temperatures currentTemp)
    {
        if (state == PAUSED)
        {
            return;
        }
        currentTemperature = currentTemp;
        Temperature dough = currentTemperature.dough;
        Temperature box = currentTemperature.box;
        bool doughNeedsCooldown = dough.isAbove(desiredDoughTemperature + 0.2f);
        bool doughNeedsBoost = dough.isBelow(desiredDoughTemperature - 0.2f);

        // this is a overall safety net
        if (box.isAbove(maxBoxTemperature))
        {
            setErrorState("Box too hot");
            return;
        }
        else if (currentRelayState == ON && millis() - lastPhaseChange > 20 * 60 * 1000)
        {
            setErrorState("Heating on for too long");
            return;
        };

        switch (state)
        {
        case START:
            if (doughNeedsCooldown)
            {
                transitionTo(COOLDOWN);
            }
            else if (dough.isWithin(holdRange()))
            {
                transitionTo(HOLD_OFF);
            }
            else if (dough.isBelow(desiredDoughTemperature))
            {
                transitionTo(BOOST_ON);
            }
            break;
        case COOLDOWN:
            if (dough.isBelow(desiredDoughTemperature))
            {
                transitionTo(HOLD_ON);
            }
            break;
        case HOLD_ON:
            if (doughNeedsCooldown)
            {
                transitionTo(COOLDOWN);
            }
            else if (doughNeedsBoost)
            {
                transitionTo(BOOST_ON);
            }
            else if (box.isAbove(holdRange()))
            {
                transitionTo(HOLD_OFF);
            }
            break;
        case HOLD_OFF:
            if (doughNeedsCooldown)
            {
                transitionTo(COOLDOWN);
            }
            else if (doughNeedsBoost)
            {
                transitionTo(BOOST_ON);
            }
            else if (box.isBelow(holdRange()))
            {
                transitionTo(HOLD_ON);
            }
            break;
        case BOOST_ON:
            if (doughNeedsCooldown)
            {
                transitionTo(COOLDOWN);
            }
            else if (dough.isAbove(desiredDoughTemperature))
            {
                transitionTo(HOLD_OFF);
            }
            else if (box.isAbove(boostRange()))
            {
                transitionTo(BOOST_OFF);
            }
            break;
        case BOOST_OFF:
            if (doughNeedsCooldown)
            {
                transitionTo(COOLDOWN);
            }
            else if (box.isBelow(boostRange()))
            {
                transitionTo(BOOST_ON);
            }
            break;
        default:
            setErrorState("Unknown state");
            break;
        }
    };

    void pause()
    {
        transitionTo(PAUSED);
    };

    void restart()
    {
        transitionTo(START);
    };

    void setErrorState(String errorReason)
    {
        transitionTo(ERROR);
        onError(errorReason);
    };

    String getStateStringified()
    {
        switch (state)
        {
        case START:
            return "START";
        case COOLDOWN:
            return "COOLDOWN";
        case HOLD_ON:
            return "HOLD_ON";
        case HOLD_OFF:
            return "HOLD_OFF";
        case BOOST_ON:
            return "BOOST_ON";
        case BOOST_OFF:
            return "BOOST_OFF";
        case ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    Temperatures getTemperatures()
    {
        return currentTemperature;
    }

    String getDesiredDoughTemperature()
    {
        return String(desiredDoughTemperature);
    }

    void setDesiredDoughTemperature(float desiredDoughTemperature)
    {
        this->desiredDoughTemperature = std::min(desiredDoughTemperature, maxDoughTemperature);
    }
};