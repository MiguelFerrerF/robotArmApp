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
  void motorOffsetChanged(int motorIndex, int newOffset);


private slots:
  void on_pushButtonReset_clicked();
  void on_pushButtonSetup_clicked();
  void on_pushButtonAllSingle_clicked();
  void on_pushButtonSetOffsets_clicked();

private:
  Ui::RobotControlDialog *ui;
  RobotConfig::RobotSettings *m_robotSettings;
  void connectSlidersToLineEdits();
  void connectLineEditsToSliders();
  void setLineEditToSliderValue(int motorIndex, int value);

  bool m_isAll = true;
};

#endif // ROBOTCONTROLDIALOG_H
