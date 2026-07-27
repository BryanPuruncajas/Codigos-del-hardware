/**
 * @file CustomConfig.cpp
 * @author David Saldana
 * @brief Custom class for creating custom configuration parameters to be used by robot.
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/IConfig.h"
#include <Preferences.h>

// another_t definition
typedef struct {
    int mode;
    double volume;
} custom_t;

// Specific configuration class for another_t
class CustomConfig : public IConfig<custom_t> {
private:
    custom_t configData;

public:
    /**
     * @copydoc IConfig::loadConfiguration()
     * 
     */
    void loadConfiguration() override {
        // Assume this loads from a different source, like a file, or different preferences
    }

    /**
     * @copydoc IConfig::getConfiguration()
     */
    const custom_t& getConfiguration() const override {
        return configData;
    }
};
