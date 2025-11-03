#ifndef VIDEOCALIBRATIONDIALOG_H
#define VIDEOCALIBRATIONDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QString>

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
  void on_pushButtonSelectDirectory_clicked();
  void on_pushButtonCaptureImage_clicked();

private:
  Ui::VideoCalibrationDialog* ui;

  QPixmap m_currentPixmap;
  QString m_selectedDirectoryPath;

  void updateVideoLabel();
  void updateFilesList();
};
#endif // VIDEOCALIBRATIONDIALOG_H