/**
 * @file RobotStateMachine.h
 * @author Swarms Lab
 * @brief Overarching class that manages all the states and handles the transition between states
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "RobotState.h"

class RobotStateMachine {
    private:
        RobotState* currentState;

    public:
        /**
         * @brief Construct a new Robot State Machine object
         * 
         * @param initState the initial state of the robot when the code is first ran
         */
        RobotStateMachine(RobotState* initState) : currentState(initState) {}

        /**
         * @brief Destroy the Robot State Machine object
         * 
         */
        ~RobotStateMachine() {
            delete currentState;
        }

        /**
         * @brief Function to update state based on sensors and controls
         * 
         * @param sensors Values received from the sensors attacted to the robot
         * @param controls Control commands from the ground station
         * @param outControls Result of conversion from control commands to proper behavior of state
         */
        void update(float sensors[], float controls[], float outControls[]) {
            RobotState* nextState = currentState->update(sensors, controls, outControls);
            if (nextState != currentState) {
                delete currentState;
                currentState = nextState;
            }
        }
};
