#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H
#include <opencv2/opencv.hpp>
#include <QObject>

class RobotHandler : public QObject {
  Q_OBJECT
public:
  static RobotHandler &instance();
  
  // Función para actualizar las matrices con los ángulos de los motores
  void actualizarMatrices();

  // Matrices de transformación
  cv::Mat RTb1;
  cv::Mat RT12;
  cv::Mat RT23;
  cv::Mat RT35;

  // Transformación final 4x4
  cv::Mat RTbt;

  // Constantes de la geometría del robot
  double a1 = 70;
  double a2 = 120;
  double a3 = 125;
  double a5 = 130;

signals:
  void errorOccurred(const QString &error);
  void matrixsUpdated(cv::Mat RTbt); 
  void messageOccurred(const QString& message);
  void motorAngleChanged(int motorIndex, int angle);
  void allMotorsReset();

private:
  explicit RobotHandler(QObject *parent = nullptr);
  ~RobotHandler();
};

#endif // ROBOTHANDLER_H
