#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H
#include "RobotConfig.h"
#include <QObject>
#include <opencv2/opencv.hpp>

class RobotHandler : public QObject
{
  Q_OBJECT
public:
  explicit RobotHandler(QObject* parent = nullptr, RobotConfig::RobotSettings* settings = nullptr);
  ~RobotHandler();
  // Funci�n para actualizar las matrices con los �ngulos de los motores
  void actualizarMatrices(const cv::Mat& q);

  // Funci�n para realizar la cinem�tica inversa
  void inverseCinematic(const cv::Point3d& efectorGlobal);

  cv::Point3d transformarPunto(const cv::Point3d& puntoLocal);

  // Matrices de transformaci�n
  cv::Mat RTb1;
  cv::Mat RT12;
  cv::Mat RT23;
  cv::Mat RT35;

  // Transformaci�n final 4x4
  cv::Mat RTbt;

  // Constantes de la geometr�a del robot
  double a1 = 130;
  double a2 = 125;
  double a3 = 125;
  double a5 = 130;

private slots:
  void onDataReceived(const QByteArray& data);
  void onDataSent(const QByteArray& data);

signals:
  void errorOccurred(const QString& error);
  void matrixsUpdated(cv::Mat RTbt);
  void messageOccurred(const QString& message);
  void motorAngleChanged(int motorIndex, int angle);
  void motorOffsetsChanged(int motorIndex, int offset);
  void allMotorsReset();
  void efectorPositionChanged(double x, double y, double z);
  void anglesCalculated(int q1, int q2, int q3, int q5);

private:
  // Matriz de �ngulos de los servomotores
  cv::Mat                     q;
  bool                        m_serialConnected = false;
  RobotConfig::RobotSettings* m_robotSettings;
};

#endif // ROBOTHANDLER_H
