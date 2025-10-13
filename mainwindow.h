#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "library-log/LogHandler.h"
#include "library-robot/RobotConfig.h"
#include "library-robot/RobotControlDialog.h"
#include "library-serial/SerialMonitorDialog.h"
#include "library-serial/SerialPortHandler.h"
#include "library-video/VideoCameraHandler.h"
#include "library-video/VideoConnectionDialog.h"
#include <QImage>
#include <QMainWindow>
#include <QPixmap>
#include <QSettings>
#include <QVideoFrame>
#include <QVideoSink>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:

  void on_actionSerial_triggered();
  void on_actionConnectSerial_triggered();
  void on_actionDisconnectSerial_triggered();
  void on_actionConnectVideo_triggered();
  void on_actionDisconnectVideo_triggered();
  void on_actionControl_triggered();
  void on_actionCalibrateRobot_triggered();

  // Serial Monitor
  void onSerialError(const QString &error);
  void onSerialStatusChanged(bool connected);
  void onSetupConnectionError(const QString &error);
  void onSerialMonitorWarning(const QString &warning);

  // Robot Control
  void onRobotControlError(const QString &error);
  void onRobotMotorAngleChanged(int motorIndex, int angle);
  void onAllMotorsReset();

private:
  Ui::MainWindow *ui;
  SerialMonitorDialog *m_SerialMonitorDialog = nullptr;
  RobotControlDialog *m_RobotControl = nullptr;
  QVideoSink *m_videoSink = nullptr;

  QSettings m_settings;
  RobotConfig::RobotSettings m_robotSettings;

  void setupConnections();
};
#endif // MAINWINDOW_H
