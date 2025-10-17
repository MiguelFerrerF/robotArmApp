#include "RobotHandler.h"
#include <cmath>
#include <QDebug>
#include <QSettings>
#include "RobotConfig.h"

#include <opencv2/core.hpp>

RobotConfig::RobotSettings robotSettings; // instancia global

RobotHandler &RobotHandler::instance() {
  static RobotHandler instance;
  return instance;
}

// Constructor
RobotHandler::RobotHandler(QObject* parent)
    : QObject(parent)
{
    // Inicializa matrices como identidad 4x4
    RTb1 = cv::Mat::eye(4, 4, CV_64F);
    RT12 = cv::Mat::eye(4, 4, CV_64F);
    RT23 = cv::Mat::eye(4, 4, CV_64F);
    RT35 = cv::Mat::eye(4, 4, CV_64F);
    RTbt = cv::Mat::eye(4, 4, CV_64F);
}


RobotHandler::~RobotHandler() {}

// Función que rellena las matrices usando los ángulos de RobotConfig
void RobotHandler::actualizarMatrices()
{
    using namespace RobotConfig;

    // Tomamos los ángulos actuales de cada motor y los convertimos a radianes
    double q1 = robotSettings.motors[0].desiredAngle * M_PI / 180.0;
    double q2 = robotSettings.motors[1].desiredAngle * M_PI / 180.0;
    double q3 = robotSettings.motors[2].desiredAngle * M_PI / 180.0;
    double q4 = robotSettings.motors[3].desiredAngle * M_PI / 180.0;

    emit messageOccurred(QString("Updating matrices with angles (degrees): q1=%1, q2=%2, q3=%3, q4=%4")
        .arg(robotSettings.motors[0].desiredAngle)
        .arg(robotSettings.motors[1].desiredAngle)
        .arg(robotSettings.motors[2].desiredAngle)
        .arg(robotSettings.motors[3].desiredAngle));

    // RTb1
    RTb1 = cv::Mat::eye(4, 4, CV_64F);
    RTb1.at<double>(1, 1) = cos(q1);
    RTb1.at<double>(1, 2) = -sin(q1);
    RTb1.at<double>(2, 1) = sin(q1);
    RTb1.at<double>(2, 2) = cos(q1);
    RTb1.at<double>(0, 3) = a1;

    // RT12
    RT12 = cv::Mat::eye(4, 4, CV_64F);
    RT12.at<double>(0, 0) = cos(q2);
    RT12.at<double>(0, 2) = sin(q2);
    RT12.at<double>(2, 0) = -sin(q2);
    RT12.at<double>(2, 2) = cos(q2);
    RT12.at<double>(0, 3) = a2;

    // RT23
    RT23 = cv::Mat::eye(4, 4, CV_64F);
    RT23.at<double>(0, 0) = cos(q3);
    RT23.at<double>(0, 2) = sin(q3);
    RT23.at<double>(2, 0) = -sin(q3);
    RT23.at<double>(2, 2) = cos(q3);
    RT23.at<double>(0, 3) = a3;

    // RT34
    RT35 = cv::Mat::eye(4, 4, CV_64F);
    RT35.at<double>(0, 0) = cos(q4);
    RT35.at<double>(0, 2) = -sin(q4);
    RT35.at<double>(2, 0) = sin(q4);
    RT35.at<double>(2, 2) = cos(q4);
    RT35.at<double>(0, 3) = a5;

    // Transformación final
    RTbt = RTb1 * RT12 * RT23 * RT35;

    emit messageOccurred("Matrices updated successfully.");
    emit matrixsUpdated(RTbt);
}
