/**
 * @file NiclaState.cpp
 * @author Swarms Lab
 * @brief Implementation of NiclaState.h
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "state/nicla/NiclaState.h"

NiclaState::NiclaState() {
    // Use NiclaConfig to access the previous history data stored
    hist = NiclaConfig::getInstance()->getDynamicHistory();

    // Gets the parameters stored in non-volatile storage of the ESP32
    const nicla_t& config = NiclaConfig::getInstance()->getConfiguration();

    // Copy the config data
    terms = config;
    
}

RobotState* NiclaState::update(float sensors[], float controls[], float outControls[]) {
    // Implement the expected behavior of the current state
    behavior(sensors, controls, outControls);

    // Determine whether a state transition is necessary and return the new state if needed
    RobotState* ptr = statetransitions(sensors, controls);

    // Store the current Nicla flags for future use in dynamic history
    hist->last_tracking_x = (float)sensors[NICLA_OFFSET + 1];
    hist->last_tracking_y =(float)sensors[NICLA_OFFSET + 2];
    hist->last_detection_w = (float)sensors[NICLA_OFFSET + 7];
    hist->last_detection_h = (float)sensors[NICLA_OFFSET + 8];
    hist->nicla_flag = (int)sensors[NICLA_OFFSET + 0];

    return ptr;
}

int NiclaState::detected(float sensors[]) {
    int detected = 0;
    int nicla_flag = (int)sensors[NICLA_OFFSET + 0];

    if (!(nicla_flag & DETECTED) && (hist->nicla_flag & DETECTED)) {
        // old flag indicated a detection while the new flag says no detection
        // that is a negative edge
        detected = -1;
    } else {
        if (nicla_flag & DETECTED != hist->nicla_flag & DETECTED) {
            // the last two MSBs of the flag toggles between 0b01 and 0b10 for new detections,
            // and it toggles to 0b00 for new no-detection
            detected = 1;
        }
    }
    return detected;
}

bool NiclaState::closeToGoal(float sensors[]) {
    float detection_w = (float)sensors[NICLA_OFFSET + 7];
    float detection_h = (float)sensors[NICLA_OFFSET + 8];

    float sideLength = max(detection_h, detection_w);
    bool too_close = true;
    // if the height of the goal is less than 75% of the height, then it is not too close;
    if (sideLength < terms.h_ratio * (float)(terms.n_max_y)) {
        too_close = false;
    }
    return too_close;
}
    


