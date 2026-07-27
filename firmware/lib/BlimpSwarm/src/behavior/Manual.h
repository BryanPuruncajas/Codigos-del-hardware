//
// Created by eddie on 3/14/24.
//

#ifndef BLIMPSWARM_MANUAL_H
#define BLIMPSWARM_MANUAL_H


#include "BehaviorInterface.h"

class Manual : public BehaviorInterface {
public:
    // Virtual destructor to ensure derived class destructors are called
    Manual();

    // behave is the function that controls the behavior of the blimp
    // it returns an integer that indicates a state transistion for other behaviors to read
    int behave(float sensors[], float controls[], float outControls[]) override;

    // reads from Preferences library to initialize variables
    void getPreferences() override;


};


#endif //BLIMPSWARM_MANUAL_H
