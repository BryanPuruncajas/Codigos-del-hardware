/**
 * @file ChargeGoal.cpp
 * @author Edward Jeff
 * @brief Implementation of ChargeGoal.h
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/nicla/ChargeGoal.h"
#include "state/nicla/ManualState.h"
#include "state/nicla/LevyWalk.h"

RobotState* ChargeGoal::statetransitions(float sensors[], float controls[]) {
    if (controls[0] < 2) {
        // If the ground station requests the robot to transition to manual
        hist->z_estimator = sensors[1];
        RobotState* manualState = new ManualState();
        return manualState;
    } else {
        // In all other states
        if (millis() - charge_timer > terms.charge_time * 1000) {
            hist->num_captures += 1;
            if (hist->num_captures >= terms.num_charges 
                || millis() - hist->start_ball_time > terms.time_in_mode * 1000) {
                // If the number of charges exceeds the maximum amount or
                // if the time in balloon mode exceeds the desired amount
                hist->num_captures = 0;
                hist->nicla_desired = !hist->nicla_desired;
                hist->start_ball_time = millis();
            }
            
            hist->z_estimator = sensors[1];
            RobotState* levyWalk = new LevyWalk();
            return levyWalk;
        } else if(hist->nicla_desired == 1 && millis() - charge_timer > (terms.charge_time * 1000) * 0.8) {
            // If the robot is in goal mode and half of the charge time has passed
            hist->z_estimator = initial_height + 2;
            return this;
        } else {
            return this; // Stay in ChargeGoal state
        }
    }
}

void ChargeGoal::behavior(float sensors[], float controls[], float outControls[]) {
    int edge = detected(sensors);
    if (hist->nicla_desired == 0 && edge == 1) { // Continue to track yaw during balloon mode
        // if a new detection is fed in
        float _yaw = sensors[5];
        float _height = sensors[1];
        int nicla_flag = (int)sensors[NICLA_OFFSET + 0];
        float tracking_x = (float)sensors[NICLA_OFFSET + 5];
        float tracking_y = (float)sensors[NICLA_OFFSET + 2];
        float detection_y = (float)sensors[NICLA_OFFSET + 6];
        float x_cal = tracking_x / terms.n_max_x; // normalizes the pixles into a value between [0,1]

        hist->des_yaw = ((x_cal - 0.5)) * terms.x_strength;
        hist->robot_to_goal = _yaw + hist->des_yaw;
    }
    outControls[0] = controls[0]; //ready
    outControls[1] = terms.fx_charge; //fx
    outControls[2] = hist->z_estimator; //fz
    outControls[3] = 0; //tx
    outControls[4] = hist->robot_to_goal; //tz
}


ChargeGoal::ChargeGoal() : NiclaState() {
    charge_timer = millis();
}

ChargeGoal::ChargeGoal(int initial_height) : initial_height(initial_height) {
    charge_timer = millis();
}


