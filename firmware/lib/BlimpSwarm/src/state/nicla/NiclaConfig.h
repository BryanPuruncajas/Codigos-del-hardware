/**
 * @file NiclaConfig.h
 * @author David Saldana
 * @brief Configuration file containing functions and variables to store parameters 
 * for the Nicla Vision and State Machine
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#include <Preferences.h>
#include "state/IConfig.h"
#include "util/DataTypes.h"


class NiclaConfig : public IConfig<nicla_t> {
    public:
        /**
         * @brief Loads the Nicla parameters saved onto the ESP32 into local variables
         * 
         */
        void loadConfiguration() override;

        /**
         * @brief Selects the specific parameters based on the current state of the robot
         * 
         * @return const nicla_t& Returns the specific parameters based on state
         */
        const nicla_t& getConfiguration() const override;

        /**
         * @brief Gets the values that were previously stored
         * 
         * @return hist_t* Returns a pointer to the struct containing the prior values
         */
        hist_t* getDynamicHistory();

        /**
         * @brief Creates and instance of the NiclaConfig type if it doesn't exist already
         * 
         * @return NiclaConfig* Returns a pointer to a new NiclaConfig struct
         */
        static NiclaConfig* getInstance();
    private:
        /**
         * @brief Construct a new Nicla Config object
         * 
         */
        NiclaConfig();

        static NiclaConfig* instance;

        // Config data for goal mode
        nicla_t configData;

        // Config data for ball mode
        nicla_t configDatab;

        // Data type to store variables from the previous iteration
        hist_t historyData;
};
