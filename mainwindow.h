#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "library-log/LogHandler.h"
#include "library-serial/SerialMonitorDialog.h"
#include "library-serial/SerialPortHandler.h"
#include <QMainWindow>
#include <QSettings>


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

  // Serial Monitor
  void onSerialError(const QString &error);
  void onSerialStatusChanged(bool connected);
  void onSetupConnectionError(const QString &error);
  void onSerialMonitorWarning(const QString &warning);

private:
  Ui::MainWindow *ui;
  SerialMonitorDialog *m_SerialMonitorDialog = nullptr;

  QSettings m_settings;
};
#endif // MAINWINDOW_H
