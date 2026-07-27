/**
 * @file CustomBicopter.h
 * @author Edward Jeff
 * @brief Class representing a template for future robot algorithms to be implemented
 * @version 0.1
 * @date 2024-03-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_CUSTOMBICOPTER_H
#define BLIMPSWARM_CUSTOMBICOPTER_H



#include "FullBicopter.h"
#include <Arduino.h>




class CustomBicopter : public FullBicopter {
public:
    /**
     * @brief Construct a new Custom Bicopter object
     * 
     */
    CustomBicopter();

    /**
     * @copydoc FullBicopter::control()
     */
    void control(float sensors[MAX_SENSORS], float controls[], int size ) override;

    /**
     * @copydoc FullBicopter::getPreferences()
     * 
     */
    void getPreferences() override;
    
private:
    /**
     * @copydoc FullBicopter::addFeedback()
     */
    void addFeedback(float sensors[MAX_SENSORS], float controls[], float feedbackControls[]);

    /**
     * @copydoc FullBicopter::getOutputs()
     */
    void getOutputs(float sensors[MAX_SENSORS], float controls[], float outputs[]);
    
    typedef struct custom_s {
        bool info;
    } custom_t;
    // Contains all the ground station constants
    custom_s customItems;
};


#endif //BLIMPSWARM_FULLBICOPTER_H
