#ifndef ROBOTCONTROLDIALOG_H
#define ROBOTCONTROLDIALOG_H

#include <QDialog>

namespace Ui {
class RobotControlDialog;
}

class RobotControlDialog : public QDialog {
  Q_OBJECT

public:
  explicit RobotControlDialog(QWidget *parent = nullptr);
  ~RobotControlDialog();

private:
  Ui::RobotControlDialog *ui;
  void connectSlidersToLabels();
};

#endif // ROBOTCONTROLDIALOG_H
