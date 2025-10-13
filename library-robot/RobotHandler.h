#ifndef ROBOTHANDLER_H
#define ROBOTHANDLER_H

#include <QObject>

class RobotHandler : public QObject {
  Q_OBJECT
public:
  static RobotHandler &instance();
  

signals:
  void errorOccurred(const QString &error);
  void motorAngleChanged(int motorIndex, int angle);
  void allMotorsReset();

private:
  explicit RobotHandler(QObject *parent = nullptr);
  ~RobotHandler();
};

#endif // ROBOTHANDLER_H
