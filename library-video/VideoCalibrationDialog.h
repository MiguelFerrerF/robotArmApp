#ifndef VIDEOCALIBRATIONDIALOG_H
#define VIDEOCALIBRATIONDIALOG_H

#include "VideoCaptureHandler.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QThread>
#include <opencv2/opencv.hpp>

namespace Ui
{
class VideoCalibrationDialog;
}

struct VideoCalibrationResult
{
  double   rms = -1.0;
  cv::Mat  cameraMatrix;
  cv::Mat  distCoeffs;
  cv::Mat  newCameraMatrix; // Matriz óptima
  cv::Rect roi;             // Región de interés
  int      processedCount = 0;
};
Q_DECLARE_METATYPE(VideoCalibrationResult)

struct Ray
{
  cv::Point3f origin;
  cv::Point3f direction;
};

struct Plane
{
  cv::Point3f normal;
  float       d; // distancia al origen
};

Ray generateRayFromPixel(const cv::Point2f& pixel, const cv::Mat& K);

cv::Point3f pixelToPlaneIntersection(const cv::Point2f& pixel, const cv::Mat& K, const cv::Point3f& planeNormal, float d);

// Esta clase contiene la lógica de calibración que se ejecutará en segundo plano
class VideoCalibrationWorker : public QObject
{
  Q_OBJECT
public:
  VideoCalibrationWorker(QObject* parent = nullptr) : QObject(parent)
  {
    qRegisterMetaType<VideoCalibrationResult>();
  }

public slots:
  // Slot que será llamado por el hilo principal para iniciar la tarea
  void doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize);

signals:
  // Señales para enviar resultados al hilo principal (VideoCalibrationDialog)
  void calibrationFinished(const VideoCalibrationResult& result);
  void calibrationError(const QString& message);
  void progressUpdate(const QString& message); // Para mostrar el progreso

private:
  // Métodos de calibración movidos del VideoCalibrationDialog
  std::vector<cv::Point3f> createObjectPoints(cv::Size boardSize, float squareSize) const;
  bool                     processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners);
  bool runCalibration(cv::Size boardSize, std::vector<std::vector<cv::Point2f>>& imagePoints, std::vector<std::vector<cv::Point3f>>& objectPoints,
                      VideoCalibrationResult& result);
  void saveCalibration(const std::string& cameraMatrixFile, const std::string& distCoeffsFile, const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                       const cv::Mat& newCameraMatrix) const;
};

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

  // Nuevos slots para recibir la respuesta del Worker
  void on_calibrationFinished(const VideoCalibrationResult& result);
  void on_calibrationError(const QString& message);
  void on_progressUpdate(const QString& message);
  void on_pushButtonGetPoint_clicked();

private:
  Ui::VideoCalibrationDialog* ui;

  QPixmap m_currentPixmap;
  QString m_selectedDirectoryPath;

  cv::Size m_calibrationBoardSize = cv::Size(9, 6); // Tamaño del tablero de ajedrez (número de esquinas interiores)
  float    m_squareSize           = 10.0f;          // Tamaño real de cada cuadrado en mm

  cv::Mat m_cameraMatrix;    // Matriz de cámara
  cv::Mat m_distCoeffs;      // Coeficientes de distorsión
  cv::Mat m_newCameraMatrix; // Matriz de cámara óptima cargada

  cv::Mat k;

  // Miembros para gestionar el hilo de trabajo
  QThread*                m_workerThread = nullptr;
  VideoCalibrationWorker* m_worker       = nullptr;

  void updateVideoLabel();
  void updateFilesList();
  void displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix, double rms);

  bool loadCalibration(const std::string& filename);
  void loadExistingCalibration();
};
#endif // VIDEOCALIBRATIONDIALOG_H