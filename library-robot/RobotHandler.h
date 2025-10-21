#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H
#include <opencv2/opencv.hpp>
#include <QObject>

class RobotHandler : public QObject {
  Q_OBJECT
public:  

	explicit RobotHandler(QObject* parent = nullptr);
	~RobotHandler();
  // Funci�n para actualizar las matrices con los �ngulos de los motores
	void actualizarMatrices(const cv::Mat& q);

  // Matrices de transformaci�n
  cv::Mat RTb1;
  cv::Mat RT12;
  cv::Mat RT23;
  cv::Mat RT35;

  // Transformaci�n final 4x4
  cv::Mat RTbt;

  // Constantes de la geometr�a del robot
  double a1 = 70;
  double a2 = 120;
  double a3 = 125;
  double a5 = 130;

private slots:
	void onDataReceived(const QByteArray& data);
	void onDataSent(const QByteArray& data);

signals:
  void errorOccurred(const QString &error);
  void matrixsUpdated(cv::Mat RTbt); 
  void messageOccurred(const QString& message);
  void motorAngleChanged(int motorIndex, int angle);
  void allMotorsReset();

private:
  //Matriz de �ngulos de los servomotores
  cv::Mat q;
  bool m_serialConnected = false;
};

#endif // ROBOTHANDLER_H
