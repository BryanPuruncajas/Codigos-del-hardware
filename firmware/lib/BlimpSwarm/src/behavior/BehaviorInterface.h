//
// Created by eddie on 3/14/24.
//

#ifndef BLIMPSWARM_BEHAVIORINTERFACE_H
#define BLIMPSWARM_BEHAVIORINTERFACE_H


class BehaviorInterface {
public:
    // Virtual destructor to ensure derived class destructors are called
    virtual ~BehaviorInterface() {}

    // behave is the function that controls the behavior of the blimp
    // it returns an integer that indicates a state transistion for other behaviors to read
    virtual int behave(float sensors[], float controls[], float outControls[]);

    // reads from Preferences library to initialize variables
    virtual void getPreferences();


};


#endif //BLIMPSWARM_BEHAVIORINTERFACE_H
