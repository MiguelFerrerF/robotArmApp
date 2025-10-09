#ifndef VIDEOCONNECTIONDIALOG_H
#define VIDEOCONNECTIONDIALOG_H

#include <QDialog>

namespace Ui {
class VideoConnectionDialog;
}

class VideoConnectionDialog : public QDialog {
  Q_OBJECT

public:
  explicit VideoConnectionDialog(QWidget *parent = nullptr);
  ~VideoConnectionDialog();

signals:
  void errorOccurred(const QString &error);

private slots:
  void on_pushButtonConnect_clicked();
  void refreshCameras();

protected:
  void showEvent(QShowEvent *event) override;

private:
  Ui::VideoConnectionDialog *ui;
};

#endif // VIDEOCONNECTIONDIALOG_H
