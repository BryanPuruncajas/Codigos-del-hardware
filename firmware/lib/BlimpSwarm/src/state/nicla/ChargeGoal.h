/**
 * @file ChargeGoal.h
 * @author Edward Jeff
 * @brief Implements the "Charging" state in which the robot charges towards a target rapidly
 * @version 0.1
 * @date 2024-06-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/nicla/NiclaState.h"

class ChargeGoal : public NiclaState {
    public:
        /**
         * @brief Construct a new Charge Goal object
         * 
         */
        ChargeGoal();

        ChargeGoal(int initial_height);
    protected:
        int initial_height = 0;
        unsigned long charge_timer = 0;
        /**
         * @copydoc NiclaState::statetransitions()
         * 
         * @brief Can transition to either Manual or LevyWalk state
         */
        RobotState* statetransitions(float sensors[], float controls[]);

        /**
         * @copydoc NiclaState::behavior()
         * 
         * @brief Charges towards an object rapidly without any interference from sensors
         */
        void behavior(float sensors[], float controls[], float outControls[]);
};