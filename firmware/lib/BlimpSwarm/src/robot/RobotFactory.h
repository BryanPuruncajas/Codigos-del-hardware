/**
 * @file RobotFactory.h
 * @author David Saldana
 * @brief Factory that produces different versions of robots
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_ROBOTFACTORY_H
#define BLIMPSWARM_ROBOTFACTORY_H


#include "Arduino.h"
#include "Robot.h"

// Include robots
#include "RawBicopter.h"
#include "FullBicopter.h"
#include "CustomBicopter.h"
#include "SBlimp.h"
#include "Differential.h"


class RobotFactory {
public:
    /**
     * @brief Create a Robot object of the specified type.
     * 
     * @param type The type of robot to be created.
     * @return Robot* The robot object that is created and returned.
     */
    static Robot* createRobot(const String& type);
};



#endif //BLIMPSWARM_ROBOTFACTORY_H
