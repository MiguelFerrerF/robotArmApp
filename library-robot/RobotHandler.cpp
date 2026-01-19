/**
 * @file RobotHandler.cpp
 * @author Miguel Ferrer
 * @brief   Handler for Robot Kinematics and Serial Communication.
 *
 * This file implements the RobotHandler class which manages
 * the kinematic calculations (Forward and Inverse Kinematics)
 * and processes serial data received from the microcontroller.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "RobotHandler.h"
#include "../library-serial/SerialPortHandler.h"
#include <QDebug>
#include <QSettings>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

RobotHandler::RobotHandler(QObject* parent, RobotConfig::RobotSettings* settings) : QObject(parent), m_robotSettings(settings)
{

  RTb1 = cv::Mat::eye(4, 4, CV_64F);
  RT12 = cv::Mat::eye(4, 4, CV_64F);
  RT23 = cv::Mat::eye(4, 4, CV_64F);
  RT35 = cv::Mat::eye(4, 4, CV_64F);
  RTbt = cv::Mat::eye(4, 4, CV_64F);

  q = (cv::Mat_<int>(1, 6) << 0, 0, 0, 0, 0, 0);

  SerialPortHandler& serial = SerialPortHandler::instance();
  connect(&serial, &SerialPortHandler::dataReceived, this, &RobotHandler::onDataReceived);
  connect(&serial, &SerialPortHandler::dataSent, this, &RobotHandler::onDataSent);

  m_serialConnected = serial.isConnected();
}

RobotHandler::~RobotHandler()
{
}

/**
 * @brief Processes ASCII protocol messages received from the microcontroller.
 *
 * Supported Commands:
 * - `ANGLE_WITH_OFFSET:SERVOx:y`: Updates the logical angle for servo X.
 * - `OFFSET:SERVOx:y`: Updates the calibration offset for servo X.
 * - `ANGLE:SERVOx:y`: Updates the raw current physical angle.
 * - `PLACE:SERVOx:y`: Updates the target "Place" position configuration.
 *
 * @param[in] data The raw bytes received via UART.
 */
void RobotHandler::onDataReceived(const QByteArray& data)
{
  const QString msg = QString::fromUtf8(data).trimmed();

  int servoNum = 0, valor = 0;

  if (sscanf(msg.toUtf8().constData(), "ANGLE_WITH_OFFSET:SERVO%d:%d", &servoNum, &valor) == 2) {
    if (servoNum < 1 || servoNum > 6) {
      emit errorOccurred(QString("Invalid servo index: %1").arg(servoNum));
      return;
    }
    if (valor < -180 || valor > 180) {
      emit errorOccurred(QString("Invalid angle: %1").arg(valor));
      return;
    }

    q.at<int>(0, servoNum - 1) = valor;
    actualizarMatrices(q);
    m_robotSettings->motors[servoNum - 1].fixedAngle = valor;

    emit motorAngleChanged(servoNum, valor);
  }
  else if (sscanf(msg.toUtf8().constData(), "OFFSET:SERVO%d:%d", &servoNum, &valor) == 2) {
    if (servoNum < 1 || servoNum > 6) {
      qDebug() << "[RobotHandler] Servo inválido:" << servoNum;
      emit errorOccurred(QString("Invalid servo index: %1").arg(servoNum));
      return;
    }
    m_robotSettings->motors[servoNum - 1].defaultAngle = valor;
    emit motorOffsetsChanged(servoNum, valor);
  }
  else if (sscanf(msg.toUtf8().constData(), "ANGLE:SERVO%d:%d", &servoNum, &valor) == 2) {
    if (servoNum < 1 || servoNum > 6) {
      emit errorOccurred(QString("Invalid servo index: %1").arg(servoNum));
      return;
    }
    if (valor < -180 || valor > 180) {
      emit errorOccurred(QString("Invalid angle: %1").arg(valor));
      return;
    }
    m_robotSettings->motors[servoNum - 1].currentAngle = valor;
  }
  else if (sscanf(msg.toUtf8().constData(), "PLACE:SERVO%d:%d", &servoNum, &valor) == 2) {
    if (servoNum < 1 || servoNum > 6) {
      emit errorOccurred(QString("Invalid servo index: %1").arg(servoNum));
      return;
    }
    m_robotSettings->motors[servoNum - 1].placePosition = valor;
    emit messageOccurred(QString("Place position for servo %1 set to %2").arg(servoNum).arg(valor));
  }
  else {
    qDebug() << "[RobotHandler] Mensaje no reconocido:" << msg;
  }
}

/**
 * @brief Updates the kinematic chain matrices based on the provided joint angles.
 *
 * This function performs the Forward Kinematics calculation .
 * It constructs the transformation matrix for each link using the robot's physical dimensions (a1, a2, a3, a5)
 * and the current rotation angles ($q$).
 *
 * The total transformation is calculated by multiplying the link matrices.
 * Finally, it computes the global position of the end-effector (Forward Kinematics)
 * and triggers an inverse kinematics check.
 *
 * @param[in] q Input matrix containing joint angles in degrees.
 */
void RobotHandler::actualizarMatrices(const cv::Mat& q)
{
  if (q.cols < 4) {
    qDebug() << "La matriz q no tiene suficientes columnas.";
    return;
  }

  // Convert angles from degrees to radians for trigonometric functions
  double q1_rad = -q.at<int>(0, 0) * M_PI / 180.0;
  double q2_rad = -q.at<int>(0, 1) * M_PI / 180.0;
  double q3_rad = -q.at<int>(0, 2) * M_PI / 180.0;
  double q5_rad = -q.at<int>(0, 4) * M_PI / 180.0;

  // RTb1 – Base to Link 1
  RTb1                  = cv::Mat::eye(4, 4, CV_64F);
  RTb1.at<double>(0, 0) = cos(q1_rad);
  RTb1.at<double>(0, 1) = -sin(q1_rad);
  RTb1.at<double>(1, 0) = sin(q1_rad);
  RTb1.at<double>(1, 1) = cos(q1_rad);
  RTb1.at<double>(2, 3) = -a1; // Z translation

  //  RT12 – Link 1 to Link 2
  RT12                  = cv::Mat::eye(4, 4, CV_64F);
  RT12.at<double>(0, 0) = cos(q2_rad);
  RT12.at<double>(0, 2) = sin(q2_rad);
  RT12.at<double>(2, 3) = -a2;
  RT12.at<double>(2, 0) = -sin(q2_rad);
  RT12.at<double>(2, 2) = cos(q2_rad);

  // RT23 – Link 2 to Link 3
  RT23                  = cv::Mat::eye(4, 4, CV_64F);
  RT23.at<double>(0, 0) = cos(q3_rad);
  RT23.at<double>(0, 2) = sin(q3_rad);
  RT23.at<double>(2, 3) = -a3;
  RT23.at<double>(2, 0) = -sin(q3_rad);
  RT23.at<double>(2, 2) = cos(q3_rad);

  // RT35 – Link 3 to End Effector
  cv::Mat RT35          = cv::Mat::eye(4, 4, CV_64F);
  RT35.at<double>(0, 0) = cos(q5_rad);
  RT35.at<double>(0, 2) = sin(q5_rad);
  RT35.at<double>(2, 3) = -a5;
  RT35.at<double>(2, 0) = -sin(q5_rad);
  RT35.at<double>(2, 2) = cos(q5_rad);

  // Compute Total Transformation
  // Note: The multiplication order here dictates the kinematic chain hierarchy
  RTbt = RT35 * RT23 * RT12 * RTb1;

  emit messageOccurred("Matrices updated successfully.");
  emit matrixsUpdated(RTbt);

  qDebug() << "Matrices actualizadas:";
  for (int i = 0; i < RTbt.rows; ++i) {
    QString row;
    for (int j = 0; j < RTbt.cols; ++j) {
      row += QString::number(RTbt.at<double>(i, j), 'f', 3) + " ";
    }
    qDebug() << row;
  }

  // Calculate Forward Kinematics: Where is the gripper now?
  cv::Point3d efectorLocal(0, 0, 0);
  cv::Point3d efectorGlobal = transformarPunto(efectorLocal);

  emit efectorPositionChanged(efectorGlobal.x, efectorGlobal.y, efectorGlobal.z);

  // Recalculate inverse kinematics for validation/update
  inverseCinematic(efectorGlobal);
}

/**
 * @brief Calculates the Inverse Kinematics using a geometric approach.
 *
 * This method solves the "Reaching" problem :
 * Given a target (X, Y, Z), it determines the required angles for the Base ($q1$),
 * Shoulder ($q2$), and Elbow ($q3$) and the Wrist angle ($q5$) to reach that point.
 *
 * **Algorithm Steps:**
 * 1. **Cylindrical Conversion:** Converts (X, Y) to radial distance $R$ and base angle $q1$.
 * 2. **Height Adjustment:** Adjusts Z relative to the shoulder axis.
 * 3. **Triangle Inequality:** Checks if the target is physically reachable.
 * If $Dist > (L1 + L2)$, the point is unreachable.
 * 4. **Law of Cosines:** Solves the triangle formed by the upper arm ($a2$) and forearm ($a3$)
 * to find the internal elbow angle ($B$) and shoulder elevation ($A$).
 *
 * @param[in] efectorGlobal Target coordinates in the robot's base frame.
 */
void RobotHandler::inverseCinematic(const cv::Point3d& efectorGlobal)
{
  // Calculate basic coordinates
  double R_val = sqrt(efectorGlobal.x * efectorGlobal.x + efectorGlobal.y * efectorGlobal.y);
  double Z_val = efectorGlobal.z;

  // Define relative height wrt shoulder (Axis 2)
  double z_rel = Z_val - a1 + a5;

  // Calculate Target Distance (Hypotenuse)
  double distancia_objetivo = sqrt(R_val * R_val + z_rel * z_rel);

  // Geometric Validation (Triangle Inequality)
  double alcance_max = a2 + a3;
  double alcance_min = fabs(a2 - a3);
  double epsilon     = 0.1;

  if (distancia_objetivo > (alcance_max + epsilon) || distancia_objetivo < (alcance_min - epsilon)) {
    qDebug() << "[RobotHandler] CRITICAL: Punto fuera del alcance físico.";
    qDebug() << "  Distancia requerida:" << distancia_objetivo;
    qDebug() << "  Alcance máximo:" << alcance_max;
    // emit errorOccurred("Target point is out of reach (Triangle Inequality violation)");
    return;
  }

  // Law of Cosines for Elbow Angle
  double cos_angle_B = (distancia_objetivo * distancia_objetivo - a2 * a2 - a3 * a3) / (2 * a2 * a3);

  // Clamp cos_angle_B to [-1, 1] to avoid NaN from acos due to floating-point errors
  if (cos_angle_B > 1.0)
    cos_angle_B = 1.0;
  if (cos_angle_B < -1.0)
    cos_angle_B = -1.0;

  double B_rad = acos(cos_angle_B);
  int    B     = round(B_rad * 180.0 / M_PI); // Usar round para mejor precisión que el truncamiento implícito

  // Law of Cosines for Shoulder Angle
  double numerador_A   = R_val * (a2 + a3 * cos(B_rad)) - a3 * sin(B_rad) * z_rel;
  double denominador_A = a2 * a2 + a3 * a3 + 2 * a2 * a3 * cos(B_rad);
  double A_rad         = asin(numerador_A / denominador_A);
  int    A             = round(A_rad * 180.0 / M_PI);

  // Calculate Wrist Angle to maintain end-effector orientation
  int C = 180 - A - B;

  // Base Rotation Angle
  double q1_rad = atan2(efectorGlobal.y, efectorGlobal.x);
  int    q1     = round(q1_rad * 180.0 / M_PI);

  emit anglesCalculated(q1, A, B, C);
}

/**
 * @brief Applies the inverse kinematic chain to map a local point to global space.
 *
 * @note This function applies `RTbt.inv()`. Depending on the matrix definition,
 * this transforms from Tool Frame to Base Frame (or vice versa).
 *
 * @param[in] puntoLocal Point in the local (effector) frame.
 * @return cv::Point3d Point in the global (base) frame.
 */
cv::Point3d RobotHandler::transformarPunto(const cv::Point3d& puntoLocal)
{
  // Create homogeneous point [x, y, z, 1]
  cv::Mat puntoHom = (cv::Mat_<double>(4, 1) << puntoLocal.x, puntoLocal.y, puntoLocal.z, 1);

  // Apply transformation
  cv::Mat puntoGlobal = RTbt.inv() * puntoHom;

  return cv::Point3d(puntoGlobal.at<double>(0, 0), puntoGlobal.at<double>(1, 0), puntoGlobal.at<double>(2, 0));
}

/**
 * @brief Logs data sent over the serial port for debugging.
 *
 * @param[in] data The raw bytes that were sent.
 */
void RobotHandler::onDataSent(const QByteArray& data)
{
  if (m_serialConnected) {
    qDebug() << "[Serial] Data sent:" << QString::fromUtf8(data);
  }
}
