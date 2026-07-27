/**
 * @file Spiral.h
 * @author Swarms Lab
 * @brief Spiral algorithm which systematically searches
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/nicla/NiclaState.h"

class Spiral : public NiclaState {
    public:
        /**
         * @brief Construct a new Spiral object
         * 
         */
        Spiral();
    protected:
        unsigned long spiralDuration = 0;
        float SpiralYaw = 0;
        unsigned long SpiralTimer = 0;
        
        /**
         * @copydoc NiclaState::statetransitions()
         * 
         * @brief Can transition of Manual or MoveToGoal state. Transitions to MoveToGoal
         * desired target is detected.
         */
        RobotState* statetransitions(float sensors[], float controls[]);

        /**
         * @copydoc NiclaState::behavior()
         * 
         * @brief Performs a spiral that grows in size over time. Useful for testing detection
         * in an enclosed space.
         */
        void behavior(float sensors[], float controls[], float outControls[]);
};