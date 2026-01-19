/**
 * @file mainwindow.h
 * @author Miguel Ferrer
 * @brief   The central hub of the Robot Arm Controller application.
 *
 * The MainWindow class is responsible for:
 * 1. **Lifecycle Management**: Instantiating and managing all child dialogs (Video, Calibration, Control).
 * 2. **Subsystem Integration**: connecting the **Computer Vision** pipeline outputs (Coordinates) to the **Robot Kinematics** inputs (Joint Angles).
 * 3. **User Interface**: Displaying the main dashboard, video feed, and status logs.
 * 4. **Command Dispatch**: Sending final high-level commands (e.g., "Pick and Place") to the microcontroller via the Serial Handler.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "library-log/LogHandler.h"
#include "library-robot/RobotCalibrationDialog.h"
#include "library-robot/RobotControlDialog.h"
#include "library-robot/RobotHandler.h"
#include "library-serial/SerialMonitorDialog.h"
#include "library-video/VideoCalibrationDialog.h"
#include "library-video/VideoManagerDialog.h"
#include "library-video/VideoProcessingDialog.h"
#include <QImage>
#include <QMainWindow>
#include <QPixmap>
#include <QSettings>
#include <QVideoFrame>
#include <QVideoSink>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief The central hub of the Robot Arm Controller application.
 *
 * The MainWindow class is responsible for:
 * 1. **Lifecycle Management**: Instantiating and managing all child dialogs (Video, Calibration, Control).
 * 2. **Subsystem Integration**: connecting the **Computer Vision** pipeline outputs (Coordinates) to the **Robot Kinematics** inputs (Joint Angles).
 * 3. **User Interface**: Displaying the main dashboard, video feed, and status logs.
 * 4. **Command Dispatch**: Sending final high-level commands (e.g., "Pick and Place") to the microcontroller via the Serial Handler.
 */
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

private slots:
  // --- Menu Actions ---
  void on_actionSerial_triggered();
  void on_actionLog_triggered();
  void on_actionConnectSerial_triggered();
  void on_actionDisconnectSerial_triggered();
  void on_actionConnectVideo_triggered();
  void on_actionDisconnectVideo_triggered();
  void on_actionCalibrationVideo_triggered();
  void on_actionMenuDocs_triggered();

  /**
   * @brief Activates the Video Processing pipeline.
   * Connects the Vision System signals to the Robot System slots to enable
   * automatic object detection and localization.
   */
  void on_actionProcessingVideo_triggered();

  void on_actionControlRobot_triggered();
  void on_actionCalibrateRobot_triggered();

  // --- Main Dashboard Controls ---

  /**
   * @brief Toggles between "Live View" and "Processing Mode".
   * When checked, it intercepts the video feed to run object detection algorithms
   * before displaying the frame.
   */
  void on_pushButtonStartProcessing_toggled(bool checked);

  /**
   * @brief Executes the Pick and Place sequence.
   * Collects the calculated joint angles (Q1-Q5) and sends the `PLACE` command
   * sequence to the robot hardware.
   */
  void on_pushButtonPickAndPlace_clicked();

  // --- Robot Signals Integration ---

  /**
   * @brief Updates the UI with the calculated end-effector position (Forward Kinematics).
   */
  void onEfectorPositionChanged(double x, double y, double z);

  /**
   * @brief Updates the UI with the required joint angles (Inverse Kinematics).
   * Triggered automatically when the vision system detects a piece and calculates its 3D position.
   */
  void onRobotAnglesCalculated(int q1, int q2, int q3, int q5);

  // --- Serial Monitor Events ---
  void onSerialError(const QString& error);
  void onSerialStatusChanged(bool connected);
  void onSetupConnectionError(const QString& error);
  void onSerialMonitorWarning(const QString& warning);
  void onDataReceived(const QByteArray& data);

  // --- Robot Control Events ---
  void onRobotControlError(const QString& error);

  /**
   * @brief Handles manual slider changes from the Robot Control Dialog.
   * Sends immediate commands (e.g., `SETUP:SERVO1:90`) to the hardware.
   */
  void onRobotMotorAngleChanged(int motorIndex, int angle);
  void onAllMotorsReset();

  /**
   * @brief Updates the UI when the physical robot reports its position.
   * Ensures the GUI sliders match the real hardware state.
   */
  void onRobotMotorAngleUpdatedFromSerial(int motorIndex, int angle);

  void onRobotMotorOffsetsReadFromMemory(int motorIndex, int offset);
  void onRobotMotorOffsetChanged(int motorIndex, int newOffset);
  void onRobotPlacePositionChanged(int motorIndex, int position);

  // --- Video Capture Events ---

  /**
   * @brief Displays the latest video frame on the main UI label.
   */
  void onVideoCapture(const QImage& image);

  void onCameraStarted();
  void onCameraStopped();
  void onCameraError(const QString& error);
  void onCameraInfoChanged(const CameraInfo& info);

private:
  Ui::MainWindow* ui;

  // Child Dialogs (Lazy loaded)
  SerialMonitorDialog*    m_SerialMonitorDialog    = nullptr;
  RobotControlDialog*     m_RobotControl           = nullptr;
  RobotCalibrationDialog* m_RobotCalibrationDialog = nullptr;
  VideoManagerDialog*     m_VideoManagerDialog     = nullptr;
  VideoCalibrationDialog* m_VideoCalibrationDialog = nullptr;
  VideoProcessingDialog*  m_VideoProcessingDialog  = nullptr;

  // Logic Handlers
  RobotHandler* m_RobotHandler = nullptr;
  QImage        m_lastCapturedFrame;

  QSettings                  m_settings;
  RobotConfig::RobotSettings m_robotSettings;

  void setupConnections();
  void connectVideoSignals();
  void disconnectVideoSignals();
};
#endif // MAINWINDOW_H
