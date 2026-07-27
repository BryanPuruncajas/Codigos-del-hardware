/**
 * @file RobotState.h
 * @author Swarms Lab
 * @brief Parent class to be inherited for the implementation of robot states
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

class RobotState {
    public:
        /**
         * @brief Destroy the Robot State object
         * 
         */
        virtual ~RobotState() {}

        /**
         * @brief Virtual function to update state based on sensors and controls
         * 
         * @param sensors Values received from the sensors attacted to the robot
         * @param controls Control commands from the ground station
         * @param outControls Result of conversion from control commands to proper behavior of state
         * @return RobotState* Returns a RobotState representing the state of the robot
         */
        virtual RobotState* update(float sensors[], float controls[], float outControls[]) = 0;

};
