/**
 * @file MoveToGoal.h
 * @author Thong Vu
 * @brief Robot state that approaches a target when detected in the environment
 * @version 0.1
 * @date 2024-06-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "state/nicla/NiclaState.h"

class MoveToGoal : public NiclaState {
    public:
        /**
         * @brief Construct a new Move To Goal object
         * 
         */
        MoveToGoal();
    protected:
        bool start = true;
        /**
         * @copydoc NiclaState::statetransitions()
         * 
         * @brief Can transition to ManualState, ChargeGoal, or LevyWalk based on the
         * detection and ground station commands.
         */
        RobotState* statetransitions(float sensors[], float controls[]);

        // moves towards the observed goal in the environment
        /**
         * @copydoc NiclaState::behavior()
         * 
         * @brief Approaches a detected object in the environment
         */
        void behavior(float sensors[], float controls[], float outControls[]);
};