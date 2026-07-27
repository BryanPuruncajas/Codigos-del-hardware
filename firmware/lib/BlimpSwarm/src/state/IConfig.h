/**
 * @file IConfig.h
 * @author Swarms Lab
 * @brief Generic parent class for loading and getting configuration parameters for the robot states
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

template<typename ConfigType>
class IConfig {
    public:
        /**
         * @brief Destroy the IConfig object
         * 
         */
        virtual ~IConfig() {}

        /**
         * @brief Loads the configurations stored in the onboard memory into variables.
         * 
         */
        virtual void loadConfiguration() = 0;

        /**
         * @brief Gets the configuration variables.
         * 
         * @return const ConfigType& Returns the data type that contains the configuration values.
         */
        virtual const ConfigType& getConfiguration() const = 0;
};
