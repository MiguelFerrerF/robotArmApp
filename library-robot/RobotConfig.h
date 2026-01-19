/**
 * @file RobotConfig.h
 * @author your name (you@domain.com)
 * @brief  Configuration structures for the Robot Arm application.
 *
 * This header defines the data structures used to represent
 * the configuration and state of the 6-axis robotic arm.
 * These structures include motor limits, default positions,
 * and runtime state variables.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef ROBOTCONFIG_H
#define ROBOTCONFIG_H

namespace RobotConfig
{

/**
 * @brief Configuration structure for a single robot motor.
 *
 * Holds the physical constraints (min/max), calibration data (default/offset),
 * and the runtime state (current/desired angles) for one axis of the robot.
 */
struct MotorConfig
{
  int minAngle      = 0;   ///< Minimum physical limit in degrees.
  int maxAngle      = 180; ///< Maximum physical limit in degrees.
  int defaultAngle  = 0;   ///< The "Home" or "Zero" position angle (offset).
  int speed         = 100; ///< Movement speed.
  int desiredAngle  = 0;   ///< The target angle set by the user interface.
  int currentAngle  = 0;   ///< The actual angle reported by the hardware/simulation.
  int fixedAngle    = 0;   ///< The angle calculated after applying offsets (used for kinematics).
  int placePosition = 0;   ///< Saved angle for the "Place" operation.
};

/**
 * @brief Represents the spatial coordinates for the robot's end-effector (claw).
 */
struct RobotClawPosition
{
  int X = 0;
  int Y = 0;
  int Z = 0;
};

/**
 * @brief Global container for the 6-axis robot configuration.
 *
 * Initializes the 6 motors with specific default offsets derived from
 * physical calibration.
 */
struct RobotSettings
{
  // Definimos cada motor con defaultAngle y offset
  MotorConfig motors[6] = {
    {0, 180, 24, 100, 0, 0},  // Motor 1
    {0, 180, 103, 100, 0, 0}, // Motor 2
    {0, 180, 20, 100, 0, 0},  // Motor 3
    {0, 180, 148, 100, 0, 0}, // Motor 4
    {0, 180, 82, 100, 0, 0},  // Motor 5
    {0, 180, 0, 100, 0, 0}    // Motor 6
  };
};

} // namespace RobotConfig

#endif // ROBOTCONFIG_H
