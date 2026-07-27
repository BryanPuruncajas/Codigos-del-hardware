/**
 * @file Robot.h
 * @author David Saldana
 * @brief Virtual class representing the skeleton for all robots
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_ROBOT_H
#define BLIMPSWARM_ROBOT_H


#define PI 3.1416

class Robot {
    public:
        // Assume a fixed maximum size for the arrays
        static const int MAX_SENSORS = 22;

        /**
         * @brief Function for retrieving sensor values
         * 
         * @param sensors Float array reprensenting the values of sensors attached to the robot
         * @return int The number of elements filled in the array
         */
        virtual int sense(float sensors[MAX_SENSORS]) = 0;

        /**
         * @brief Function for the actuation of commands
         * 
         * @param actuators Float array representing the desired values of the actuators
         * @param size The size of the array inputed
         */
        virtual void actuate(const float actuators[], int size) = 0;

        /**
         * @brief Function that starts up the robot and initializes associated variables
         * 
         */
        virtual void startup();

        /**
         * @brief Takes the sensor array, an array of higher-level control commands, and its size.
         * Converts sensor and control commands into actuation
         * 
         * @param sensors Float array of values received from sensors
         * @param controls Float array of values received from controller input
         * @param size The size of the control array
         */
        virtual void control(float sensors[MAX_SENSORS], float controls[], int size ) = 0;

        /**
         * @brief Calibrates the brushless motors
         * 
         */
        virtual void calibrate();

        /**
         * @brief Arms the brushless motors
         * 
         */
        virtual void arm();

        /**
         * @brief Reads values from non-volatile storage (NVS).
         * Must manually enter keys and default values for every variable.
         * 
         */
        virtual void getPreferences();

        /**
         * @brief Destroy the Robot object.
         * 
         */
        virtual ~Robot() {}

    private:
        // Pure virtual function for variable settings
};



#endif
