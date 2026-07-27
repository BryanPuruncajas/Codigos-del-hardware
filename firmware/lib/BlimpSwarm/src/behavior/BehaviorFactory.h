//
// Created by eddie on 3/14/24.
//

#ifndef BLIMPSWARM_BEHAVIORFACTORY_H
#define BLIMPSWARM_BEHAVIORFACTORY_H


#include "Arduino.h"
#include "BehaviorInterface.h"

// Other behavior types would be included here
#include "Manual.h"
#include "NiclaSimpleServo.h"

class BehaviorFactory {
public:
    static BehaviorInterface* createBehavior(const String& type);
};



#endif //BLIMPSWARM_BEHAVIORFACTORY_H
