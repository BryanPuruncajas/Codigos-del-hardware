    
    
#include "NiclaSimpleServo.h"


NiclaSimpleServo::NiclaSimpleServo(){

}


int NiclaSimpleServo::servoing(float sensors[], float controls[], float outControls[]) {
    
    float dt = (micros() - lastLoop)/1000000.0f;
    lastLoop = micros();
    int niclaOffset = 11;
    int nicla_flag = (int)sensors[niclaOffset + 0];
    float tracking_x = (float)sensors[niclaOffset + 1];
    float tracking_y =(float)sensors[niclaOffset + 2];
    float tracking_w = (float)sensors[niclaOffset + 3];
    float tracking_h = (float)sensors[niclaOffset + 4];
    float detection_x =(float)sensors[niclaOffset + 5];
    float detection_y = (float)sensors[niclaOffset + 6];
    float detection_w = (float)sensors[niclaOffset + 7];
    float detection_h = (float)sensors[niclaOffset + 8];

    float sideLength = max(detection_h, detection_w);
    // if (detection_y + sideLength/2 > max_y) {
    //     detection_y += (sideLength - min(detection_h, detection_w))/2;
    // } else if (tracking_y - sideLength/2 < 0) {
    //     detection_y -= (sideLength - min(detection_h, detection_w))/2;
    // }
    float _yaw = sensors[5];

    float _height = sensors[1];

    if (last_tracking_x != tracking_x || last_tracking_y != tracking_y || last_detection_w != detection_w || last_detection_h != detection_h) {
        
        float subdt = (millis() - lastDetection)/1000.0f;
        lastDetection = millis();

        float x_cal = tracking_x / max_x;
        des_yaw = ((x_cal - 0.5));
        robot_to_goal = _yaw + des_yaw;
        
        

        float y_cal = detection_y / max_y;
        if ( abs(des_yaw) < .1) { // if too close than height doesnt make sense
            // z_offset += 20*((y_cal - 0.5))* subdt / sideLength;//(.75 - max(detection_h, detection_w)/max_y);
            z_offset = z_offset * .7 + ( _height + 1 * (y_cal - 0.65)) * .3; // integral must be on
        }
        
        // if (detected == false) { // positive edge of detection
        // goal_yaw_estimator = robot_to_goal;
        //     goal_yaw_counter = 1;
        // } else { // as time goes on the yaw becomes more and more solidified. TODO: look for a better function than 1/x
        //     goal_yaw_estimator = goal_yaw_estimator * (1.0f - 1.0f/goal_yaw_counter) + robot_to_goal * (1.0f/goal_yaw_counter);
        //     goal_yaw_counter += 1;
        // }
    }
    // if it has been less than a second since the last detection, we consider it still detected
    if (millis() - lastDetection < 1000) {
        detected = true;
    } else {
        detected = false;
        z_offset= controls[2];
    }
    bool too_close = true;
    // if the height of the goal is less than 75% of the height, then it is not too close; 
    if (sideLength < .75 * max_y) {
        too_close = false;
    }

    if (state == 0) {
        if (millis() - levyTimer > levyDuration){
            levyTimer = millis();
            float lambda = 1.0 / 5000.0; // Adjust lambda for scaling; 5000 is the mean of the distribution
            float randomValue = random(1, 10001) / 10000.0; // Generate a random float between 0.0001 and 1
            unsigned long duration = (unsigned long)(-log(randomValue) / lambda);
            // Ensure the duration is within the desired range (0 to 10,000 ms)
            duration = duration % 30001; // Modulo to restrict within range if necessary
            levyDuration = duration;
            levyYaw = _yaw + random(-180, 180)/180.0f * 3.14;
        }
        if (detected) {
            levyDuration = 0;
            state = 1;
        }
    } else if (state == 1){
        if (!detected) {
            state = 0;
        } else if (too_close) {
            state = 2;
            charge_timer = millis();
            goal_yaw_estimator = robot_to_goal;
            z_estimator = z_offset;
        }
    } else if (state == 2){
        if (millis() - charge_timer > 5000) {
            state = 0;
        }
    }

    if (state == 0) { // search for goal
        control_yaw = levyYaw;
        control_x = -.1;
        control_z = controls[2];
    } else if (state == 1) {
        control_yaw = robot_to_goal;
        control_x = -0.2;
        control_z = z_offset;
    } else if (state == 2) {
        control_yaw = goal_yaw_estimator;
        control_x = -0.4;
        control_z = z_estimator;

    }


    outControls[0] = controls[0]; //ready
    outControls[1] = control_x; //fx
    outControls[2] = control_z; //fz
    outControls[3] = 0; //tx
    outControls[4] = control_yaw; //tz
    return 1;

}

// behave is the function that controls the behavior of the blimp
// it returns an integer that indicates a state transistion for other behaviors to read
int NiclaSimpleServo::behave(float sensors[], float controls[], float outControls[]) {
    if (controls[0] == 2){
        return servoing(sensors, controls, outControls);

    } else {
        return manual.behave(sensors, controls, outControls);
    }
    return 1;
}

// reads from Preferences library to initialize variables
void NiclaSimpleServo::getPreferences() {
    return;
}