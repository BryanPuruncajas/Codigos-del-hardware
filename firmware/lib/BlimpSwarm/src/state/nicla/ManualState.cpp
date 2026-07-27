/**
 * @file ManualState.cpp
 * @author Swarms Lab
 * @brief Implementation of ManualState.h
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/nicla/ManualState.h"
#include "state/nicla/LevyWalk.h"
#include "state/nicla/MoveToGoal.h"

RobotState* ManualState::statetransitions(float sensors[], float controls[]) {
    if (controls[0] < 2){
        // If the ground station requests the robot to transition to manual
        hist->z_estimator = sensors[1];
        return this;
    } else if(controls[0] != 5 && detected(sensors)) {
        hist->z_estimator = sensors[1];
        hist->robot_to_goal = sensors[5];
        RobotState* moveToGoal = new MoveToGoal();
        return moveToGoal;
    } else if(controls[0] != 5) {
        hist->z_estimator = sensors[1];
        hist->robot_to_goal = sensors[5];
        RobotState* levyWalk = new LevyWalk();
        return levyWalk;
    } else {
        hist->z_estimator = sensors[1];
        return this;
    }
}

void ManualState::behavior(float sensors[], float controls[], float outControls[]) {
    if(controls[2] != 0) {
        maintained_height = sensors[1];
        outControls[2] = sensors[1] + controls[2]*5;
    } else {
        outControls[2] = maintained_height;
    }

    if(controls[4] != 0) {
        maintained_yaw = sensors[5];
        outControls[4] = sensors[5] + controls[4]*5;
    } else {
        outControls[4] = maintained_yaw;
    }
    outControls[0] = controls[0]; //ready
    outControls[1] = controls[1]; //fx
    outControls[3] = controls[3]; //tx
    outControls[5] = controls[5];
}


ManualState::ManualState() : NiclaState() {
    
}


