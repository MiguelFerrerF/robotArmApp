#ifndef VIDEOCALIBRATIONDIALOG_H
#define VIDEOCALIBRATIONDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>

namespace Ui
{
class VideoCalibrationDialog;
}

class VideoCalibrationDialog : public QDialog
{
  Q_OBJECT

public:
  VideoCalibrationDialog(QWidget* parent = nullptr);
  ~VideoCalibrationDialog();

private slots:
  void on_startButton_clicked();

private:
  Ui::VideoCalibrationDialog* ui;

  QPixmap m_currentPixmap;

  void updateVideoLabel();
};
#endif // VIDEOCALIBRATIONDIALOG_H