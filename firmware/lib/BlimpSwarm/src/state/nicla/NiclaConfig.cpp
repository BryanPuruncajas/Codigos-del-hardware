/**
 * @file NiclaConfig.cpp
 * @author David Saldana
 * @brief Implementation of NiclaConfig.h
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "state/nicla/NiclaConfig.h"

NiclaConfig* NiclaConfig::instance = nullptr;

NiclaConfig::NiclaConfig() {
    loadConfiguration();
}

void NiclaConfig::loadConfiguration() {
    Preferences preferences;
    preferences.begin("params", true);


    historyData.nicla_flag = preferences.getInt("state_flag", 0x40);

    configData.state = 1;
    configData.num_charges = preferences.getInt("num_charges", 10);
    configData.time_in_mode = preferences.getInt("time_in_mode", 600);
    configData.charge_time = preferences.getInt("charge_time", 15);
    configData.target_color = preferences.getInt("target_color", 0x81);
    configData.default_height = preferences.getFloat("default_height", 8);
    configData.height_range = preferences.getFloat("height_range", 3);
    configData.wall_thresh = preferences.getFloat("wall_thresh", 250);

    configData.y_thresh = preferences.getFloat("y_thresh", 0.65);
    configData.y_strength = preferences.getFloat("y_strength", 1);
    configData.x_strength = preferences.getFloat("x_strength", 1);

    configData.fx_togoal = preferences.getFloat("fx_togoal", -0.2f);
    configData.fx_charge = preferences.getFloat("fx_charge", -0.4f);
    configData.fx_levy = preferences.getFloat("fx_levy", -0.1f);
    configData.fz_levy = preferences.getFloat("fz_levy", 0.1f);

    configData.n_max_x = preferences.getInt("n_max_x", 240);
    configData.n_max_y = preferences.getInt("n_max_y", 160);
    configData.h_ratio = preferences.getFloat("h_ratio", 0.75f);

    configData.range_for_forward = preferences.getFloat("range_for_forward", 0.16);

    configDatab.state = 0;
    configDatab.num_charges = preferences.getInt("bnum_charges", 4);
    configDatab.time_in_mode = preferences.getInt("btime_in_mode", 60);
    configDatab.charge_time = preferences.getInt("bcharge_time", 15);
    configDatab.target_color = preferences.getInt("btarget_color", 0x40);
    configDatab.default_height = preferences.getFloat("bdefault_height", 5);
    configDatab.height_range = preferences.getFloat("bheight_range", 3);
    configDatab.wall_thresh = preferences.getFloat("bwall_thresh", 250);

    configDatab.y_thresh = preferences.getFloat("by_thresh", 0.65);
    configDatab.y_strength = preferences.getFloat("by_strength", 1);
    configDatab.x_strength = preferences.getFloat("bx_strength", 1);

    configDatab.fx_togoal = preferences.getFloat("bfx_togoal", -0.2f);
    configDatab.fx_charge = preferences.getFloat("bfx_charge", -0.4f);
    configDatab.fx_levy = preferences.getFloat("bfx_levy", -0.1f);
    configDatab.fz_levy = preferences.getFloat("bfz_levy", 0.1f);

    configDatab.n_max_x = preferences.getInt("bn_max_x", 240);
    configDatab.n_max_y = preferences.getInt("bn_max_y", 160);
    configDatab.h_ratio = preferences.getFloat("bh_ratio", 0.75f);

    configDatab.range_for_forward = preferences.getFloat("brange_for_forward", 0.16);
    Serial.println("Loaded config");
    preferences.end();
}

const nicla_t& NiclaConfig::getConfiguration() const {
    if (historyData.nicla_desired == 1) {
        return configData;
    } else if (historyData.nicla_desired == 0) {
        return configDatab;
    } else {
        return configDatab;// temp until more states
    }
}

hist_t* NiclaConfig::getDynamicHistory() {
    return &historyData;
}

NiclaConfig* NiclaConfig::getInstance() {
    if (instance == nullptr) {
        instance = new NiclaConfig();
    }
    return instance;
}
