/**
 * @file ManualState.h
 * @author Swarms Lab
 * @brief Class that represents the manual state of the robot
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "state/nicla/NiclaState.h"


class ManualState : public NiclaState {
    public:
        /**
         * @brief Construct a new Manual State object
         * 
         */
        ManualState();
    protected:
        float maintained_height;
        float maintained_yaw;
        
        /**
         * @copydoc NiclaState::statetransitions()
         * 
         * @brief Can transition to MoveToGoal or LevyWalk. If the target is detected,
         * it will transition to MoveToGoal.
         */
        RobotState* statetransitions(float sensors[], float controls[]) ;

        /**
         * @copydoc NiclaState::behavior()
         * 
         * @brief Behavior is determined directly by the ground station input device
         */
        void behavior(float sensors[], float controls[], float outControls[]) ;
};