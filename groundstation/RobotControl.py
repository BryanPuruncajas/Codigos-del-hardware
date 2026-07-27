from comm.Serial import DataType_Boolean
from input.JoystickManager import JoystickManager
from user_parameters import ROBOT_MACS, TENSILE_MASTER, TENSILE_FOLLOWERS, DEFENDER_MACS
from robot.RobotMaster import RobotMaster
import Preferences
import time
import importlib

PRINT_JOYSTICK = False


def startAutonomousBall(serial, robot, args):
    serial.send_control_params(robot, (3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.2)
    serial.send_control_params(robot, (2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.2)
    return 2


def startAutonomousGoal(serial, robot, args):
    serial.send_control_params(robot, (4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.2)
    serial.send_control_params(robot, (2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.2)
    return 2


def stopOne(serial, robot, args):
    serial.send_control_params(robot, (5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.05)
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.05)
    return 0


def sendCalibrate(serial, robot, args):
    serial.send_control_params(robot, (5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    time.sleep(0.05)
    serial.send_preference(robot, DataType_Boolean, "calibrate", True)
    time.sleep(0.05)
    return -1


def controlTensileFollowers(serial, robot, args):
    serial.send_control_params(
        robot, (args[0], args[0], args[1], args[1], 0, 0, 0, 0, 0, 0, 0, 0, 0)
    )
    time.sleep(0.05)
    return 1


def sendPreferences(serial, robot, args):
    importlib.reload(Preferences)

    for pref in Preferences.PREFERENCES["ff:ff:ff:ff:ff:ff"]:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    if robot not in Preferences.PREFERENCES:
        serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
        return -1
    prefer = Preferences.PREFERENCES[robot]
    for pref in prefer:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
    return -1


def sendOrangePreferences(serial, robot, args):
    importlib.reload(Preferences)

    for pref in Preferences.PREFERENCES["orange"]:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    if robot not in Preferences.PREFERENCES:
        serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
        return -1
    prefer = Preferences.PREFERENCES[robot]
    for pref in prefer:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
    return -1


def sendYellowPreferences(serial, robot, args):
    importlib.reload(Preferences)

    for pref in Preferences.PREFERENCES["yellow"]:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    if robot not in Preferences.PREFERENCES:
        serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
        return -1
    prefer = Preferences.PREFERENCES[robot]
    for pref in prefer:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
    return -1


def sendDefenderPreferences(serial, robot, args):
    importlib.reload(Preferences)

    for pref in Preferences.PREFERENCES["defender"]:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    if robot not in Preferences.PREFERENCES:
        serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
        return -1
    prefer = Preferences.PREFERENCES[robot]
    for pref in prefer:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
    return -1


def sendAttackerPreferences(serial, robot, args):
    importlib.reload(Preferences)

    for pref in Preferences.PREFERENCES["attacker"]:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    if robot not in Preferences.PREFERENCES:
        serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
        return -1
    prefer = Preferences.PREFERENCES[robot]
    for pref in prefer:
        serial.send_preference(robot, pref["data_type"], pref["key"], pref["value"])
    serial.send_control_params(robot, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0))
    return -1


def main():
    try:
        robot_master = RobotMaster(0.3)
        joystick = JoystickManager()
        macs = ROBOT_MACS + DEFENDER_MACS + TENSILE_MASTER + TENSILE_FOLLOWERS
        robot_master.setup(macs, "nicla")
        followers = [
            i + 1 for i in list(range(len(macs) - len(TENSILE_FOLLOWERS), len(macs)))
        ]

        robot_master.functionFactory("s", stopOne, "Stop")
        robot_master.functionFactory("b", startAutonomousBall, "Auto Ball")
        robot_master.functionFactory("g", startAutonomousGoal, "Auto Goal")
        robot_master.functionFactory("c", sendCalibrate, "Calibrate")
        robot_master.functionFactory("p", sendPreferences, "Send Preferences")
        robot_master.functionFactory(
            "a", sendAttackerPreferences, "Send Attacker Preferences"
        )
        robot_master.functionFactory(
            "d", sendDefenderPreferences, "Send Defender Preferences"
        )
        robot_master.functionFactory(
            "f", controlTensileFollowers, "Controlling Follower"
        )
        robot_master.functionFactory(
            "y", sendYellowPreferences, "Send Yellow Preferences"
        )
        robot_master.functionFactory(
            "o", sendOrangePreferences, "Send Orange Preferences"
        )

        index = "0"
        power = 0.45
        angle = 45

        while True:
            time.sleep(0.2)
            keys = robot_master.get_last_n_keys(1)
            axis, buttons = joystick.getJoystickInputs()
            robot_master.processManual(axis, buttons, print_vals=True)

            power = min(1, max(0, power))
            angle = min(180, max(-180, angle))

            # Display joystick values if enabled
            if PRINT_JOYSTICK:
                print(" ".join(["{:.1f}".format(num) for num in axis]), buttons)

            if len(keys) == 0:
                continue

            key_pressed = keys[0]

            if key_pressed == "enter":
                i = int(index)
                robot_master.switchRobot(i)
                index = "0"
            elif len(key_pressed) == 1 and 32 <= ord(key_pressed) <= 127:
                if "0" <= key_pressed <= "9":
                    index += key_pressed
                elif key_pressed == "[":
                    for i in followers:
                        robot_master.runFunction("f", i, 0, 0)
                elif key_pressed == "]":
                    for i in followers:
                        robot_master.runFunction("f", i, power, angle)
                elif key_pressed == "=":
                    angle += 5
                    for i in followers:
                        robot_master.runFunction("f", i, power, angle)
                elif key_pressed == "-":
                    angle -= 5
                    for i in followers:
                        robot_master.runFunction("f", i, power, angle)
                elif key_pressed == ";":
                    power -= 0.05
                    for i in followers:
                        robot_master.runFunction("f", i, power, angle)
                elif key_pressed == "'":
                    power += 0.05
                    for i in followers:
                        robot_master.runFunction("f", i, power, angle)
                elif key_pressed == "s":
                    power = 0
                    angle = 45
                    i = int(index)
                    robot_master.runFunction(key_pressed, i)
                    index = "0"
                else:
                    i = int(index)
                    robot_master.runFunction(key_pressed, i)
                    index = "0"
            else:
                print("Invalid button.")
    except KeyboardInterrupt:
        print("Stopping!")
        return
    except Exception as e:
        print(e)
        return


if __name__ == "__main__":
    main()
