#include "config.h"
#include <core_esp8266_features.h>
#include <functional>
#include <algorithm>
#include <WString.h>
#include "Temperatures.h"

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
    DETENTION,
    ERROR
};

class DesiredDoughTemperature
{
public:
    float value;
    DesiredDoughTemperature(float value) : value(std::min(value, MAX_DOUGH_TEMP)) {}
};

class StateManager
{
private:
    unsigned long lastPhaseChange;
    unsigned long detentionStart;
    DesiredDoughTemperature desiredDoughTemperature;
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
        float lowerLimit = desiredDoughTemperature.value + 4;
        float upperLimit = desiredDoughTemperature.value + 6;
        return Range(lowerLimit, upperLimit);
    }
    Range holdRange()
    {
        float lowerLimit = desiredDoughTemperature.value - 0.5f;
        float upperLimit = desiredDoughTemperature.value + 2.0f;
        return Range(lowerLimit, upperLimit);
    }

public:
    StateManager(float desiredDoughTemperature, std::function<void(Relay, String)> onStateChange, std::function<void(String)> onError) :
        lastPhaseChange(millis()),
        desiredDoughTemperature(desiredDoughTemperature),
        state(START),
        currentRelayState(OFF),
        currentTemperature(Temperatures(0.0, 0.0)),
        onStateChange(onStateChange),
        onError(onError)
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
        bool doughNeedsCooldown = dough.isAbove(desiredDoughTemperature.value + 0.2f);
        bool doughNeedsBoost = dough.isBelow(desiredDoughTemperature.value - 0.2f);

        // this is a overall safety net
        if (currentRelayState == ON)
        {
            if (box.isAbove(MAX_BOX_TEMP))
            {
                detent();
                return;
            }
            else if (millis() - lastPhaseChange > 10 * 60 * 1000)
            {
                detent();
                return;
            };
        }

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
            else if (dough.isBelow(desiredDoughTemperature.value))
            {
                transitionTo(BOOST_ON);
            }
            break;
        case COOLDOWN:
            if (dough.isBelow(desiredDoughTemperature.value))
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
            else if (dough.isAbove(desiredDoughTemperature.value))
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
        case DETENTION:
            if (detentionOver())
            {
                transitionTo(START);
            }
            break;
        case ERROR:
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
        if (state != PAUSED)
        {
            transitionTo(ERROR);
            onError(errorReason);
        }
    };

    void detent()
    {
        detentionStart = millis();
        transitionTo(DETENTION);
    };

    bool detentionOver()
    {
        return millis() - detentionStart > 10 * 60 * 1000;
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
        case PAUSED:
            return "PAUSED";
        case DETENTION:
            return "DETENTION";
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
        return String(desiredDoughTemperature.value);
    }

    void setDesiredDoughTemperature(float desiredDoughTemperature)
    {
        this->desiredDoughTemperature = DesiredDoughTemperature(desiredDoughTemperature);
    }
};