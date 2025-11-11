#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H
#include <opencv2/opencv.hpp>
#include <QObject>

class RobotHandler : public QObject {
  Q_OBJECT
public:  

	explicit RobotHandler(QObject* parent = nullptr);
	~RobotHandler();
  // Función para actualizar las matrices con los ángulos de los motores
	void actualizarMatrices(const cv::Mat& q);

	// Función para realizar la cinemática inversa
	void inverseCinematic(const cv::Point3d& efectorGlobal);

	cv::Point3d transformarPunto(const cv::Point3d& puntoLocal);

  // Matrices de transformación
  cv::Mat RTb1;
  cv::Mat RT12;
  cv::Mat RT23;
  cv::Mat RT35;

  // Transformación final 4x4
  cv::Mat RTbt;

  // Constantes de la geometría del robot
  double a1 = 130;
  double a2 = 125;
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
  void motorOffsetsChanged(int motorIndex, int offset);
  void allMotorsReset();
  void efectorPositionChanged(double x, double y, double z);

private:
  //Matriz de ángulos de los servomotores
  cv::Mat q;
  bool m_serialConnected = false;
};

#endif // ROBOTHANDLER_H
