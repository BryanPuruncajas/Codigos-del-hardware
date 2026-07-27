/**
 * @file Print.h
 * @author David Saldana
 * @brief Includes a variety of printing methods for different data types.
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */


/**
 * @brief Prints Control input data type
 * 
 * @param cmd The control input data to be printed out.
 */
void printControlInput(ControlInput cmd){
    int n = sizeof(cmd.params)/ sizeof(cmd.params[0]);
    for(int i = 0; i < 5; ++i) {
        Serial.print(cmd.params[i]); // Print each byte in hexadecimal
        Serial.print(" "); // Print a space between bytes for readability
    }
    Serial.println();
}