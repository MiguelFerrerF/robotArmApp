#ifndef ROBOTCONFIG_H
#define ROBOTCONFIG_H

namespace RobotConfig {

struct MotorConfig {
  int minAngle = 0;
  int maxAngle = 180;
  int defaultAngle = 0;
  int speed = 100;
  int desiredAngle = 0;
  int currentAngle = 0;
  int fixedAngle   = 0;
};

struct RobotClawPosition { // Angles for claw 
  int X = 0;
  int Y = 0;
  int Z = 0;
};

struct RobotSettings
{
  // Definimos cada motor con defaultAngle y offset
  MotorConfig motors[6] = {
    {0, 180, 24, 100, 0, 0},   // Motor 1
    {0, 180, 103, 100, 0, 0},  // Motor 2
    {0, 180, 20, 100, 0, 0},   // Motor 3
    {0, 180, 148, 100, 0, 0},  // Motor 4
    {0, 180, 82, 100, 0, 0},   // Motor 5
    {0, 180, 0, 100, 0, 0}     // Motor 6
  };
};

} // namespace RobotConfig

#endif // ROBOTCONFIG_H
