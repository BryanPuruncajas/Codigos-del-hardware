/**
 * @file RobotFactory.cpp
 * @author David Saldana
 * @brief Factory that creates the specified type of robot.
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include "RobotFactory.h"

Robot* RobotFactory::createRobot(const String& type) {
    if (type== "RawBicopter") {
        Robot* robot = new RawBicopter();
        robot->startup();
        return robot;
    } else if (type== "FullBicopter") {
        Robot* robot = new FullBicopter();
        robot->startup();
        return robot;
    } else if (type== "CustomBicopter") {
        Robot* robot = new CustomBicopter();
        robot->startup();
        return robot;
    } else if (type== "SBlimp") {
        Robot* robot = new SBlimp();
        robot->startup();
        return robot;
    }
        // Add cases for other robot types here
    else {
        return nullptr;
    }
}