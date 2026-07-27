/**
 * @file LevyWalk.h
 * @author Edward Jeff
 * @brief Class that represents a random walk algorithm which excels in uncertain environments
 * @version 0.1
 * @date 2024-06-28
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "state/nicla/NiclaState.h"

class LevyWalk : public NiclaState {
  public:
    /**
     * @brief Construct a new Levy Walk object
     *
     */
    LevyWalk();

  protected:
    float yawCurr = 0;
    bool levy = false;

    unsigned long explore_duration = 0;

    float angleProgress, yaw_rate = 0.0;
    float current_desired_yaw = 0; // Current yaw in action
    float current_fx = 0;          // Current forward force in action
    float final_desired_yaw;
    long initial_time;
    float total_yaw;
    float previous_yaw;

    /**
     * @copydoc NiclaState::statetransitions()
     *
     * @brief Can transition to Manual or MoveToGoal states. Transitions to MoveToGoal when
     * the target is detected.
     */
    RobotState *statetransitions(float sensors[], float controls[]);

    /**
     * @copydoc NiclaState::behavior()
     *
     * @brief Implements an algorithm in which the height and yaw randomly fluctuates.
     * Efficient in uncertain environments where variability is high.
     */
    void behavior(float sensors[], float controls[], float outControls[]);

  private:
    bool wallDetected = false;
    float turn_start_yaw = 0;
    float forward_force = 0;
    bool spiral_completed;
};