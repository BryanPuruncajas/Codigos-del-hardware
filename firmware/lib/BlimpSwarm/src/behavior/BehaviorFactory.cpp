//
// Created by eddie on 3/14/24.
//


#include "BehaviorFactory.h"

BehaviorInterface* BehaviorFactory::createBehavior(const String& type) {
    if (type== "Manual") {
        return new Manual();
    } else if (type== "Nicla") {
        return new NiclaSimpleServo();
    }
        // Add cases for other robot types here
    else {
        return new Manual(); // manual is default
    }
}