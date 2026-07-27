/**
 * @file LED.h
 * @author David Saldana
 * @brief Actuation of LEDS 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_LED_H
#define BLIMPSWARM_LED_H

#include "Actuator.h"

class LED : public Actuator{
public:
    /**
     * @brief Construct a new LED object
     * 
     * @param pinVal Pin used for the component
     */
    LED(int pinVal);

    /**
     * @brief Actuates the value given to the LED => (a value > 0 is on, otherwise off) 
     * 
     * @param value The value to actuate
     */
    void act(float value);
};


#endif //BLIMPSWARM_LED_H
