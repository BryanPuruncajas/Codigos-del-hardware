/**
 * @file LevyWalk.cpp
 * @author Edward Jeff
 * @brief Implementation of LevyWalk.h
 * @version 0.1
 * @date 2024-01-01
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "state/nicla/LevyWalk.h"
#include "state/nicla/ManualState.h"
#include "state/nicla/MoveToGoal.h"

LevyWalk::LevyWalk() : NiclaState() {
    hist->start_ball_time = millis();
    initial_time = millis();

    final_desired_yaw = hist->robot_to_goal;
    current_desired_yaw = hist->robot_to_goal;
    yaw_rate = 0.4;
    total_yaw = 0;
    previous_yaw = 0;
    spiral_completed = true;
    explore_duration = 0;
}

RobotState *LevyWalk::statetransitions(float sensors[], float controls[]) {
    if (controls[0] < 2) {
        // If the ground station requests the robot to transition to manual
        hist->z_estimator = sensors[1];
        RobotState *manualState = new ManualState();
        return manualState;
    } else if (terms.state != hist->nicla_desired) {
        // If the current state is different than the desired state
        RobotState *levyWalk = new LevyWalk();
        return levyWalk;
    } else if (detected(sensors)) {
        // If the target is detected
        float _yaw = sensors[5];
        int nicla_flag = (int)sensors[NICLA_OFFSET + 0];
        float tracking_x = (float)sensors[NICLA_OFFSET + 1];

        float x_cal = tracking_x / terms.n_max_x; // normalizes the pixels into a value between [0,1]

        hist->des_yaw = ((x_cal - 0.5)) * terms.x_strength;
        hist->robot_to_goal = _yaw + hist->des_yaw;

        RobotState *moveToGoal = new MoveToGoal();
        return moveToGoal;
    } else if (sensors[11] < terms.wall_thresh) {
        // Wall detected, initiate turn
        wallDetected = true;
        turn_start_yaw = sensors[5]; // Store current yaw
        return this;                 // Stay in LevyWalk state, but with wall avoidance active
    } else if (millis() - hist->start_ball_time > terms.time_in_mode * 1000) {
        // If the robot has been in LevyWalk state for more than the designated time
        hist->num_captures = 0;
        hist->nicla_desired = !hist->nicla_desired;
        hist->start_ball_time = millis();
        return this;
    } else {
        hist->robot_to_goal = sensors[5];
        return this; // pointer to itself
    }
}

void LevyWalk::behavior(float sensors[], float controls[], float outControls[]) {
    unsigned long current_time = millis();
    float current_yaw = sensors[5];
    if (wallDetected) {
        // Calculate the target yaw (90 degrees turn)
        final_desired_yaw = turn_start_yaw + M_PI / 2.0; // Add 90 degrees (in radians)

        // Normalize the target yaw to be between -π and π
        while (final_desired_yaw > M_PI)
            final_desired_yaw -= 2 * M_PI;
        while (final_desired_yaw < -M_PI)
            final_desired_yaw += 2 * M_PI;

        // Calculate the difference
        float yaw_diff = final_desired_yaw - current_yaw;

        // Normalize the difference to be between -π and π
        while (yaw_diff > M_PI)
            yaw_diff -= 2 * M_PI;
        while (yaw_diff < -M_PI)
            yaw_diff += 2 * M_PI;

        // If we're close to the target yaw, stop turning
        if (abs(yaw_diff) < 0.5) { // 0.1 radians is about 5.7 degrees
            explore_duration = 5000;
            initial_time = current_time;
            wallDetected = false;
        } else {
            current_desired_yaw = final_desired_yaw;
        }
    } else {
        unsigned long elapsed_time = current_time - initial_time;
        if (elapsed_time >= explore_duration && spiral_completed) {
            initial_time = current_time;
            float additional_loops = (random(0, 1000) / 1000.0) * (4 * M_PI);
            final_desired_yaw = 2 * M_PI + additional_loops;

            explore_duration = random(5000, 10000);

            float random_height = terms.fz_levy * random(-10000, 10000) / 10000.0;
            hist->z_estimator = sensors[1] + random_height;
            spiral_completed = false;
            total_yaw = 0;
            previous_yaw = current_yaw;
        } else if (elapsed_time >= explore_duration) {
            float yaw_diff = current_yaw - previous_yaw;

            while (yaw_diff > M_PI)
                yaw_diff -= 2 * M_PI;
            while (yaw_diff < -M_PI)
                yaw_diff += 2 * M_PI;

            total_yaw += yaw_diff;
            previous_yaw = current_yaw;

            if (total_yaw >= final_desired_yaw) {
                spiral_completed = true;
            } else {
                float current_yaw_rate = ((final_desired_yaw - total_yaw) / final_desired_yaw) * yaw_rate;
                current_yaw_rate = constrain(current_yaw_rate, 0.2, 0.4);
                current_desired_yaw = current_yaw + current_yaw_rate;
            }
        }

        if (sensors[1] >= terms.default_height + terms.height_range || sensors[1] <= terms.default_height - terms.height_range) {
            hist->z_estimator = terms.default_height;
        }
    }

    // Set control outputs for both behaviors
    outControls[0] = controls[0];         // ready
    outControls[1] = terms.fx_levy;;          // fx
    outControls[2] = hist->z_estimator;   // fz
    outControls[3] = 0;                   // Other control
    outControls[4] = current_desired_yaw; // Set the updated yaw based on behavior
}