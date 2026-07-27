/**
 * @file MoveToGoal.cpp
 * @author Swarms Lab
 * @brief Implementation of MoveToGoal.h
 * @version 0.1
 * @date 2024-01-01
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "state/nicla/MoveToGoal.h"
#include "state/nicla/ChargeGoal.h"
#include "state/nicla/LevyWalk.h"
#include "state/nicla/ManualState.h"

RobotState *MoveToGoal::statetransitions(float sensors[], float controls[]) {
    int nicla_flag = (int)sensors[NICLA_OFFSET + 0];
    if (controls[0] < 2) {
        // If the ground station requests the robot to transition to manual
        hist->z_estimator = sensors[1];
        RobotState *manualState = new ManualState();
        return manualState;
    } else if (terms.state != hist->nicla_desired) {
        // reload terms for important movements
        RobotState *moveToGoal = new MoveToGoal();
        return moveToGoal;
    } else if (hist->nicla_flag & GOAL_MODE) { // TODO: CLEAN UP THIS LOGIC
        if (start == true) {
            return this;
        } else if (!(nicla_flag & DETECTED)) {
            // no detection registered by nicla in flag
            RobotState *levyWalk = new LevyWalk();
            return levyWalk;
        } else if (closeToGoal(sensors)) {
            RobotState *chargeGoal = new ChargeGoal(hist->z_estimator);
            return chargeGoal;
        } else {
            return this; // pointer to itself
        }
    } else if (hist->nicla_flag & BALLOON_MODE) {
        // state transition for balloon detection
        if (!(nicla_flag & DETECTED)) { // no detection registered by nicla in flag
            RobotState *levyWalk = new LevyWalk();
            return levyWalk;
        } else if (closeToGoal(sensors) && abs(hist->last_tracking_x / terms.n_max_x - 0.5) < terms.range_for_forward * 0.6) {
            RobotState *chargeGoal = new ChargeGoal(hist->z_estimator);
            return chargeGoal;
        } else {
            return this; // pointer to itself
        }
    } else {
        start = true;
        return this;
    }
}

void MoveToGoal::behavior(float sensors[], float controls[], float outControls[]) {
    int edge = detected(sensors);
    // if (start){
    //     float _yaw = sensors[5];

    //     hist->robot_to_goal = _yaw;
    //     hist->forward_force = 0;
    // }

    if (edge == 1) {
        start = false;
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
        float y_cal = detection_y / terms.n_max_y;
        if (abs(x_cal - 0.5) < terms.range_for_forward) { // makes sure yaw is in center before making height adjustments
            // z_offset += 20*((y_cal - 0.5))* subdt / sideLength;//(.75 - max(detection_h, detection_w)/max_y);
            hist->z_estimator = (_height + terms.y_strength * (y_cal - terms.y_thresh)); // integral must be on
            hist->forward_force = terms.fx_togoal;
        }

        if (sensors[1] >= terms.default_height + terms.height_range || sensors[1] <= terms.default_height - terms.height_range) {
            hist->z_estimator = terms.default_height;
        }
    }
    outControls[0] = controls[0];         // ready
    outControls[1] = hist->forward_force; // fx
    outControls[2] = hist->z_estimator;   // fz
    outControls[3] = 0;                   // tx
    outControls[4] = hist->robot_to_goal; // tz
}

MoveToGoal::MoveToGoal() : NiclaState() {
    hist->forward_force = 0.0;
    start = true;
}