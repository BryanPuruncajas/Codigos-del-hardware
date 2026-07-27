    
    
#include "Manual.h"


Manual::Manual(){

}

// behave is the function that controls the behavior of the blimp
// it returns an integer that indicates a state transistion for other behaviors to read
int Manual::behave(float sensors[], float controls[], float outControls[]) {
    
    for (int i = 0; i < 6; i++){
        outControls[i] = controls[i];

    }
    
    return 1;
}

// reads from Preferences library to initialize variables
void Manual::getPreferences() {
    return;
}