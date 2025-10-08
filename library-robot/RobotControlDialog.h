#ifndef ROBOTCONTROLDIALOG_H
#define ROBOTCONTROLDIALOG_H

#include "RobotConfig.h"
#include <QDialog>

namespace Ui {
class RobotControlDialog;
}

class RobotControlDialog : public QDialog {
  Q_OBJECT

public:
  explicit RobotControlDialog(QWidget *parent = nullptr,
                              RobotConfig::RobotSettings *settings = nullptr);
  ~RobotControlDialog();

signals:
  void errorOccurred(const QString &error);
  void motorAngleChanged(int motorIndex, int angle);
  void allMotorsReset();

private slots:
  void on_pushButtonReset_clicked();
  void on_pushButtonSetup_clicked();

private:
  Ui::RobotControlDialog *ui;
  RobotConfig::RobotSettings *m_robotSettings;
  void connectSlidersToLabels();
  void setLabelToSliderValue(int motorIndex, int value);
};

#endif // ROBOTCONTROLDIALOG_H
