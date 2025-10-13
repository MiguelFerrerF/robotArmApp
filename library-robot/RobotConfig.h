#ifndef ROBOTCONFIG_H
#define ROBOTCONFIG_H

namespace RobotConfig {

struct MotorConfig {
  int minAngle = 0;
  int maxAngle = 180;
  int defaultAngle = 90;
  int speed = 100;
  int desiredAngle = 0;
  int currentAngle = 0;
};

struct RobotClawPosition { // Angles for claw 
  int X = 0;
  int Y = 0;
  int Z = 0;
};

struct RobotSettings {
  MotorConfig motors[6];
};

} // namespace RobotConfig

#endif // ROBOTCONFIG_H
