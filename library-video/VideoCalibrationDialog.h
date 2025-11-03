#ifndef VIDEOCALIBRATIONDIALOG_H
#define VIDEOCALIBRATIONDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <opencv2/opencv.hpp>

namespace Ui {
class VideoCalibrationDialog;
}

class VideoCalibrationDialog : public QDialog {
  Q_OBJECT

public:
  VideoCalibrationDialog(QWidget *parent = nullptr);
  ~VideoCalibrationDialog();

private slots:
  void on_startButton_clicked();
  void on_pushButtonSelectDirectory_clicked();
  void on_pushButtonCaptureImage_clicked();

private:
  Ui::VideoCalibrationDialog *ui;

  QPixmap m_currentPixmap;
  QString m_selectedDirectoryPath;

  cv::Size m_calibrationBoardSize = cv::Size(
      9, 6); // Tamaño del tablero de ajedrez (número de esquinas interiores)
  float m_squareSize = 10.0f; // Tamaño real de cada cuadrado en mm
  std::vector<std::vector<cv::Point2f>>
      m_imagePoints; // Puntos 2D detectados en las imágenes
  std::vector<std::vector<cv::Point3f>>
      m_objectPoints; // Puntos 3D correspondientes en el espacio del mundo

  cv::Mat m_cameraMatrix; // Matriz de cámara
  cv::Mat m_distCoeffs;   // Coeficientes de distorsión
  cv::Mat m_rvec, m_tvec; // Vectores de rotación y traslación

  void updateVideoLabel();
  void updateFilesList();

  std::vector<cv::Point3f> createObjectPoints() const;
  bool processImageForCorners(const cv::Mat &image);
  bool runCalibration();
  void saveCalibration(const std::string &cameraMatrixFile,
                       const std::string &distCoeffsFile) const;
  bool loadCalibration(const std::string &filename);
  void loadExistingCalibration();
};
#endif // VIDEOCALIBRATIONDIALOG_H
