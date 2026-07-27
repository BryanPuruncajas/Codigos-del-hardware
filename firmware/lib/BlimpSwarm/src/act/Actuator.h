/**
 * @file Actuator.h
 * @author David Saldana
 * @brief Parent class for actuation of robot components
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_ACTUATOR_H
#define BLIMPSWARM_ACTUATOR_H

class Actuator {
public:
    /**
     * @brief Construct a new Actuator object.
     * 
     * @param pinVal The pin used to actuate the component.
     */
    Actuator(int pinVal) : min(0), max(1), offset(0), pin(pinVal){
    }

    /**
     * @brief Construct a new Actuator object.
     * 
     * @param minVal Minimum value that can be actuated.
     * @param maxVal Maximum value that can be actuated.
     * @param offsetVal Offset value to adjust the range.
     * @param pinVal The pin used to actuate the component.
     */
    Actuator(int minVal, int maxVal, int offsetVal, int pinVal)
            : min(minVal), max(maxVal), offset(offsetVal), pin(pinVal){
    }

    /**
     * @brief Set the offset value.
     * 
     * @param newOffset The new offset value to be used.
     */
    void setOffset(int newOffset) {
        offset = newOffset;
    }

    /**
     * @brief Actuates the component.
     * 
     * @param value The value to actuate.
     */
    virtual void act(float value) = 0;

protected:
    int min;
    int max;
    int offset;
    int pin;
};


#endif //BLIMPSWARM_ACTUATOR_H
