/**
 * @file NiclaState.h
 * @author Swarms Lab
 * @brief Class that contains the 'structure' of the nicla state, any function that needs to be called by any state, 
 * as well as organizing the state parameters and history values from the NiclaConfig.h file.
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef NICLA_STATE
#define NICLA_STATE

#include "state/RobotStateMachine.h"
#include "state/nicla/NiclaConfig.h"


// Offset to find the flags sent from the Nicla Vision
#define NICLA_OFFSET 12 // 12 = 11 (sensorSuite) + 1 (ultrasonicSensor)
#define WallTurnRange 0.523599 // 45 degrees
#define BALLOON_MODE 0x40
#define GOAL_MODE 0x80
#define DETECTED 0b11

class NiclaState : public RobotState {
    public:
        /**
         * @brief Construct a new Nicla State object. Also gathers the config file data.
         * 
         */
        NiclaState();

        /**
         * @brief Function to update state based on sensors and controls.
         * 
         * @param sensors Values received from the sensors attacted to the robot
         * @param controls Control commands from the ground station
         * @param outControls Result of conversion from control commands to proper behavior of state
         * @return RobotState* Returns a RobotState representing the state of the robot
         */
        RobotState* update(float sensors[], float controls[], float outControls[]);

    protected:
        // Variable that keeps track of prior sensor values
        hist_t* hist;
        // Additional parameter values to be stored in non-volatile part of ESP32 memory
        // Unlike other parameter values, these values change based on the state of the robot
        nicla_t terms; 

        /**
         * @brief Function that checks a flag indicating whether the Nicla has detected a desired object.
         * Detects the positive and negative edges of a new image.
         * 
         * @param sensors Values representing the flags from the sensors and Nicla Vision
         * @return int Integer representing whether an object was detected
         */
        int detected(float sensors[]);

        /**
         * @brief Detects whether a robot is a certain distance to a target.
         * 
         * @param sensors Values representing the flags from the sensors and Nicla Vision
         * @return true Robot is past the threshold
         * @return false Robot is not past the threshold
         */
        bool closeToGoal(float sensors[]);
        
        /**
         * @brief Function that evaluates and determines the state that the robot should be in.
         * 
         * @param sensors Values representing the flags from the sensors and Nicla Vision
         * @param controls Control commands from the ground station
         * @return RobotState* Returns a RobotState representing the state of the robot
         */
        virtual RobotState* statetransitions(float sensors[], float controls[]) = 0;

        /**
         * @brief Function that represents how a robot should behave in it's current state.
         * 
         * @param sensors Values representing the flags from the sensors and Nicla Vision
         * @param controls Control commands from the ground station
         * @param outControls Result of conversion from control commands to proper behavior of state
         */
        virtual void behavior(float sensors[], float controls[], float outControls[]) = 0;
};

#endif // NICLA_STATE