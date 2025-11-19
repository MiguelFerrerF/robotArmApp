#ifndef ROBOTCALIBRATIONDIALOG_H
#define ROBOTCALIBRATIONDIALOG_H

#include "../library-video/VideoCaptureHandler.h"
#include "RobotConfig.h"
#include <QDialog>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QThread>
#include <opencv2/opencv.hpp>

namespace Ui
{
class RobotCalibrationDialog;
}

struct RobotCalibrationResult
{
  double   rms = -1.0;
  cv::Mat  cameraMatrix;
  cv::Mat  distCoeffs;
  cv::Mat  newCameraMatrix; // Matriz óptima
  cv::Rect roi;             // Región de interés
  int      processedCount = 0;
  cv::Mat RTcb;
};
Q_DECLARE_METATYPE(RobotCalibrationResult)

// Esta clase contiene la lógica de calibración que se ejecutará en segundo
// plano
class RobotCalibrationWorker : public QObject
{
  Q_OBJECT
public:
  RobotCalibrationWorker(QObject* parent = nullptr) : QObject(parent)
  {
    qRegisterMetaType<RobotCalibrationResult>();
  }

public slots:
  // Slot que será llamado por el hilo principal para iniciar la tarea
  void doCalibration(const QString& directoryPath, cv::Size boardSize, float squareSize);

signals:
  // Señales para enviar resultados al hilo principal (RobotCalibrationDialog)
  void calibrationFinished(const RobotCalibrationResult& result);
  void calibrationError(const QString& message);
  void progressUpdate(const QString& message); // Para mostrar el progreso

private:
  // Métodos de calibración movidos delRobotCalibrationDialog
  std::vector<cv::Point3f> createObjectPoints(cv::Size boardSize, float squareSize) const;
   bool                     processImageForCorners(const cv::Mat& image, cv::Size boardSize, float squareSize, std::vector<cv::Point2f>& corners);

  void saveCalibration(const std::string& RT_camera_base, const cv::Mat& RTcb) const;
  bool loadCalibration(RobotCalibrationResult& result);
  void getRTbaseToolFromFile(const std::string& jsonFilePath, cv::Mat& Rbt, cv::Mat& Tbt);
};

class RobotCalibrationDialog : public QDialog
{
  Q_OBJECT

public:
  RobotCalibrationDialog(QWidget* parent = nullptr, RobotConfig::RobotSettings* settings = nullptr);
  ~RobotCalibrationDialog();

private slots:
  void on_startButton_clicked();
  void on_pushButtonSelectDirectory_clicked();
  void on_pushButtonCaptureImage_clicked();

  // Nuevos slots para recibir la respuesta del Worker
  void on_calibrationFinished(const RobotCalibrationResult& result);
  void on_calibrationError(const QString& message);
  void on_progressUpdate(const QString& message);

private:
  Ui::RobotCalibrationDialog* ui;

  QPixmap m_currentPixmap;
  QString m_selectedDirectoryPath;

  cv::Size m_calibrationBoardSize = cv::Size(9, 6); // Tamaño del tablero de ajedrez (número de esquinas interiores)
  float    m_squareSize           = 10.0f;          // Tamaño real de cada cuadrado en mm

  cv::Mat m_cameraMatrix;    // Matriz de cámara
  cv::Mat m_distCoeffs;      // Coeficientes de distorsión
  cv::Mat m_newCameraMatrix; // Matriz de cámara óptima cargada

  // Miembros para gestionar el hilo de trabajo
  QThread*                m_workerThread = nullptr;
  RobotCalibrationWorker* m_worker       = nullptr;

  // Robot settings pointer
  RobotConfig::RobotSettings* m_robotSettings;

  void updateVideoLabel();
  void updateFilesList();
  void displayCalibrationResults(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, const cv::Mat& newCameraMatrix, double rms);

  bool saveMotorAnglesToJson(const RobotConfig::RobotSettings& settings, const QString& filePath);

  bool loadCalibration(const std::string& filename);
  void loadExistingCalibration();
};
#endif // ROBOTCALIBRATIONDIALOG_H
