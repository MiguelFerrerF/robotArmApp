#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H
#include "RobotConfig.h"
#include <QObject>
#include <opencv2/opencv.hpp>

/**
 * @brief Central controller for the robot's kinematics and serial communication events.
 *
 * This class acts as the bridge between the raw serial data and the high-level application logic.
 * It is responsible for:
 * 1. Maintaining the kinematic model (Forward Kinematics) using Denavit-Hartenberg matrices.
 * 2. Calculating required joint angles for target coordinates (Inverse Kinematics).
 * 3. Parsing incoming serial protocols (e.g., "ANGLE:SERVO1:90").
 * 4. Managing coordinate transformations between the Base and the End-Effector.
 */
class RobotHandler : public QObject
{
  Q_OBJECT
public:
  explicit RobotHandler(QObject* parent = nullptr, RobotConfig::RobotSettings* settings = nullptr);
  ~RobotHandler();

  /**
   * @brief Recomputes the Forward Kinematics based on the current joint angles.
   *
   * Updates the internal homogeneous transformation matrices ($RT_{b1}$ to $RT_{35}$)
   * and calculates the total transformation $RT_{bt}$.
   *
   * @param[in] q A 1x6 OpenCV matrix containing the current angles (in degrees) for servos 1-6.
   */
  void actualizarMatrices(const cv::Mat& q);

  /**
   * @brief Performs Inverse Kinematics (IK) to find joint angles for a target 3D point.
   *
   * Uses a geometric approach (trigonometry/law of cosines) to solve for the
   * shoulder (q2), elbow (q3), and base (q1) angles necessary to reach the coordinates.
   *
   * @param[in] efectorGlobal The target (X, Y, Z) coordinates in the Robot Base frame.
   */
  void inverseCinematic(const cv::Point3d& efectorGlobal);

  /**
   * @brief Transforms a point from the End-Effector's local frame to the Global Base frame.
   *
   * @param[in] puntoLocal The 3D point relative to the gripper/tool (usually 0,0,0).
   * @return cv::Point3d The coordinates of that point relative to the robot's base.
   */
  cv::Point3d transformarPunto(const cv::Point3d& puntoLocal);

  // --- Transformation Matrices (Denavit-Hartenberg) ---

  cv::Mat RTb1; ///< Transformation from Base to Link 1.
  cv::Mat RT12; ///< Transformation from Link 1 to Link 2.
  cv::Mat RT23; ///< Transformation from Link 2 to Link 3.
  cv::Mat RT35; ///< Transformation from Link 3 to End Effector.

  /**
   * @brief Total Homogeneous Transformation Matrix (Chain).
   * Represents the accumulated kinematic chain.
   */
  cv::Mat RTbt;

  // --- Robot Geometry Constants (Link Lengths in mm) ---

  double a1 = 130; ///< Height from base to shoulder pivot.
  double a2 = 125; ///< Length of the upper arm (humerus).
  double a3 = 125; ///< Length of the forearm (radius).
  double a5 = 130; ///< Length from wrist to end-effector tip.

private slots:
  /**
   * @brief Parses incoming raw data from the serial port.
   * Handles messages like "ANGLE:", "OFFSET:", and "PLACE:".
   */
  void onDataReceived(const QByteArray& data);
  void onDataSent(const QByteArray& data);

signals:
  void errorOccurred(const QString& error);
  void matrixsUpdated(cv::Mat RTbt);
  void messageOccurred(const QString& message);

  /**
   * @brief Emitted when a servo's angle is updated via serial feedback.
   * @param motorIndex 1-based index of the motor.
   * @param angle The new angle in degrees.
   */
  void motorAngleChanged(int motorIndex, int angle);

  void motorOffsetsChanged(int motorIndex, int offset);
  void allMotorsReset();

  /**
   * @brief Emitted when the calculated position of the end-effector changes (Forward Kinematics result).
   */
  void efectorPositionChanged(double x, double y, double z);

  /**
   * @brief Emitted when Inverse Kinematics successfully calculates new joint angles.
   */
  void anglesCalculated(int q1, int q2, int q3, int q5);

private:
  cv::Mat                     q; ///< Internal storage for current joint angles [1x6].
  bool                        m_serialConnected = false;
  RobotConfig::RobotSettings* m_robotSettings;
};

#endif // ROBOTHANDLER_H
