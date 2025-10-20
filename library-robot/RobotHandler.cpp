#include "RobotHandler.h"
#include "../library-serial/SerialPortHandler.h"
#include <cmath>
#include <QDebug>
#include <QSettings>
#include "RobotConfig.h"
#include <QRegularExpression>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

RobotConfig::RobotSettings robotSettings; // instancia global

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


    // Conectar se�ales del SerialPortHandler
    SerialPortHandler& serial = SerialPortHandler::instance();
    connect(&serial, &SerialPortHandler::dataReceived, this,
        &RobotHandler::onDataReceived);
    connect(&serial, &SerialPortHandler::dataSent, this,
        &RobotHandler::onDataSent);

    m_serialConnected = serial.isConnected();
}


RobotHandler::~RobotHandler() {}

void RobotHandler::actualizarMatrices(const cv::Mat& q)
{
    if (q.cols < 4) {
        qDebug() << "La matriz q no tiene suficientes columnas.";
        return;
    }

    // Convertir �ngulos de grados a radianes usando la matriz q
    double q1_rad = q.at<int>(0, 0) * M_PI / 180.0;
    double q2_rad = q.at<int>(0, 1)-12 * M_PI / 180.0;
    double q3_rad = q.at<int>(0, 2)+1 * M_PI / 180.0;
    double q4_rad = q.at<int>(0, 3)-45 * M_PI / 180.0;

    // RTb1
    RTb1 = cv::Mat::eye(4, 4, CV_64F);
    RTb1.at<double>(1, 1) = cos(q1_rad);
    RTb1.at<double>(1, 2) = -sin(q1_rad);
    RTb1.at<double>(2, 1) = sin(q1_rad);
    RTb1.at<double>(2, 2) = cos(q1_rad);
    RTb1.at<double>(0, 3) = a1;

    // RT12
    RT12 = cv::Mat::eye(4, 4, CV_64F);
    RT12.at<double>(0, 0) = cos(q2_rad);
    RT12.at<double>(0, 2) = sin(q2_rad);
    RT12.at<double>(2, 0) = -sin(q2_rad);
    RT12.at<double>(2, 2) = cos(q2_rad);
    RT12.at<double>(0, 3) = a2;

    // RT23
    RT23 = cv::Mat::eye(4, 4, CV_64F);
    RT23.at<double>(0, 0) = cos(q3_rad);
    RT23.at<double>(0, 2) = sin(q3_rad);
    RT23.at<double>(2, 0) = -sin(q3_rad);
    RT23.at<double>(2, 2) = cos(q3_rad);
    RT23.at<double>(0, 3) = a3;

    // RT34 / RT35
    RT35 = cv::Mat::eye(4, 4, CV_64F);
    RT35.at<double>(0, 0) = cos(q4_rad);
    RT35.at<double>(0, 2) = -sin(q4_rad);
    RT35.at<double>(2, 0) = sin(q4_rad);
    RT35.at<double>(2, 2) = cos(q4_rad);
    RT35.at<double>(0, 3) = a5;

    // Transformaci�n final
    RTbt = RTb1 * RT12 * RT23 * RT35;

    emit messageOccurred("Matrices updated successfully.");
    emit matrixsUpdated(RTbt);
	qDebug() << "Matrices actualizadas.";
    // Imprimir RTbt fila por fila
    for (int i = 0; i < RTbt.rows; ++i) {
        QString row;
        for (int j = 0; j < RTbt.cols; ++j) {
            row += QString::number(RTbt.at<double>(i, j), 'f', 3) + " ";
        }
        qDebug() << row;
    }
}


void RobotHandler::onDataReceived(const QByteArray& data) {
    QString text = QString::fromUtf8(data).trimmed();
    qDebug() << "Datos recibidos:" << text;

    // Expresi�n regular para el formato: [INFO] Command received: Action=SETUP, Parameter=SERVO3, Value=90
    QRegularExpression regex(R"(Parameter=SERVO(\d+),\s*Value=(\d+))");
    QRegularExpressionMatch match = regex.match(text);


    if (match.hasMatch()) {
        int servoNum = match.captured(1).toInt(); 
        int valor = match.captured(2).toInt();     

        qDebug() << "Servo" << servoNum << "-> Valor:" << valor;

        // Asignar seg�n el n�mero de servo
        switch (servoNum) {
        case 1: q1 = valor; break;
        case 2: q2 = valor; break;
        case 3: q3 = valor; break;
        case 4: q4 = valor; break;
        case 5: q5 = valor; break;
        case 6: q6 = valor; break;
        default:
            qDebug() << "Servo desconocido:" << servoNum;
            break;
        }

        // Ejemplo opcional: usar OpenCV con los valores q1..q6
        cv::Mat q = (cv::Mat_<int>(1, 6) << q1, q2, q3, q4, q5, q6);
        qDebug() << "Estado actual de los servos:"
            << "[" << q1 << "," << q2 << "," << q3 << ","
            << q4 << "," << q5 << "," << q6 << "]";

        // Actualizar los valores de la matriz de cinem�tica
        actualizarMatrices(q);
    }

    qDebug() << "Fin de data";
    return;

    m_serialConnected = SerialPortHandler::instance().isConnected();
}


void RobotHandler::onDataSent(const QByteArray& data) {
    if (m_serialConnected) {
        QString text = QString::fromUtf8(data);
        qDebug(data);
    }
}
