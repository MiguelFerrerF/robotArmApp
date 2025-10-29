#include "RobotHandler.h"
#include "../library-serial/SerialPortHandler.h"
#include <cmath>
#include <QDebug>
#include <QSettings>
#include "RobotConfig.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

RobotConfig::RobotSettings robotSettings; // instancia global

RobotHandler::RobotHandler(QObject* parent)
    : QObject(parent)
{
    // Inicializa matrices como identidad 4x4
    RTb1 = cv::Mat::eye(4, 4, CV_64F);
    RT12 = cv::Mat::eye(4, 4, CV_64F);
    RT23 = cv::Mat::eye(4, 4, CV_64F);
    RT35 = cv::Mat::eye(4, 4, CV_64F);
    RTbt = cv::Mat::eye(4, 4, CV_64F);

    // Inicializa matriz de ángulos (q1...q6)
    q = (cv::Mat_<int>(1, 6) << 0, 0, 0, 0, 0, 0);

    // Conexión de señales del puerto serie
    SerialPortHandler& serial = SerialPortHandler::instance();
    connect(&serial, &SerialPortHandler::dataReceived, this, &RobotHandler::onDataReceived);
    connect(&serial, &SerialPortHandler::dataSent, this, &RobotHandler::onDataSent);

    m_serialConnected = serial.isConnected();
}

void RobotHandler::onDataReceived(const QByteArray& data)
{
    const QString msg = QString::fromUtf8(data).trimmed();

    int servoNum = 0, valor = 0;

    // Extraer servo y valor con sscanf
    if (sscanf(msg.toUtf8().constData(), "ANGLE:SERVO%d:%d", &servoNum, &valor) == 2)
    {
        // Validar rangos
        if (servoNum < 1 || servoNum > 6) {
            qDebug() << "[RobotHandler] Servo inválido:" << servoNum;
            emit errorOccurred(QString("Invalid servo index: %1").arg(servoNum));
            return;
        }
        if (valor < -180 || valor > 180) {
            qDebug() << "[RobotHandler] Valor fuera de rango:" << valor;
            emit errorOccurred(QString("Invalid angle: %1").arg(valor));
            return;
        }

        // Actualizar el valor correspondiente en la matriz q
        q.at<int>(0, servoNum - 1) = valor;

        qDebug() << QString("[RobotHandler] Servo %1 -> %2").arg(servoNum).arg(valor);
        qDebug() << "Estado actual de los servos: ["
            << q.at<int>(0, 0) << ", "
            << q.at<int>(0, 1) << ", "
            << q.at<int>(0, 2) << ", "
            << q.at<int>(0, 3) << ", "
            << q.at<int>(0, 4) << ", "
            << q.at<int>(0, 5) << "]";

        // Actualizar matrices cinemáticas
        actualizarMatrices(q);

        // Emitir señal informando cambio de ángulo
        emit motorAngleChanged(servoNum, valor);
    }
    else {
        qDebug() << "[RobotHandler] Mensaje invalido:" << msg;
    }
}

void RobotHandler::actualizarMatrices(const cv::Mat& q)
{
    if (q.cols < 4) {
        qDebug() << "La matriz q no tiene suficientes columnas.";
        return;
    }

    // Convertir ángulos de grados a radianes
    double q1_rad = q.at<int>(0, 0) * M_PI / 180.0;
    double q2_rad = q.at<int>(0, 1) * M_PI / 180.0;
    double q3_rad = q.at<int>(0, 2) * M_PI / 180.0;
    double q5_rad = q.at<int>(0, 3) * M_PI / 180.0;

    // RTb1 – Base al primer eslabón
    RTb1 = cv::Mat::eye(4, 4, CV_64F);
    RTb1.at<double>(0, 0) = cos(q1_rad);
    RTb1.at<double>(0, 1) = -sin(q1_rad);
    RTb1.at<double>(1, 0) = sin(q1_rad);
    RTb1.at<double>(1, 1) = cos(q1_rad);
    RTb1.at<double>(2, 3) = a1; // traslación en z

    // RT12 – Primer eslabón al segundo
    RT12 = cv::Mat::eye(4, 4, CV_64F);
    RT12.at<double>(0, 0) = cos(q2_rad);
    RT12.at<double>(0, 2) = -sin(q2_rad);
    RT12.at<double>(2, 3) = a2;
    RT12.at<double>(2, 0) = sin(q2_rad);
    RT12.at<double>(2, 2) = cos(q2_rad);

    // RT23 – Segundo al tercero
    RT23 = cv::Mat::eye(4, 4, CV_64F);
    RT23.at<double>(0, 0) = cos(q3_rad);
    RT23.at<double>(0, 2) = -sin(q3_rad);
    RT23.at<double>(2, 3) = a3;
    RT23.at<double>(2, 0) = sin(q3_rad);
    RT23.at<double>(2, 2) = cos(q3_rad);

    // RT35 – Tercer eslabón al efector final
    RT35 = cv::Mat::eye(4, 4, CV_64F);
    RT35.at<double>(0, 0) = cos(q5_rad);
    RT35.at<double>(0, 2) = -sin(q5_rad);
    RT35.at<double>(2, 3) = a5;
    RT35.at<double>(2, 0) = sin(q5_rad);
    RT35.at<double>(2, 2) = cos(q5_rad);

    // Transformación total
    RTbt = RTb1 * RT12 * RT23 * RT35;

    emit messageOccurred("Matrices updated successfully.");
    emit matrixsUpdated(RTbt);

    qDebug() << "Matrices actualizadas:";
    for (int i = 0; i < RTbt.rows; ++i) {
        QString row;
        for (int j = 0; j < RTbt.cols; ++j) {
            row += QString::number(RTbt.at<double>(i, j), 'f', 3) + " ";
        }
        qDebug() << row;
    }

    cv::Point3d efectorLocal(0, 0, 0);
    cv::Point3d efectorGlobal = transformarPunto(efectorLocal);

    qDebug() << "Posicion de la pinza (respecto a la base del robot):"
        << "(" << efectorGlobal.x << ","
        << efectorGlobal.y << ","
        << efectorGlobal.z << ")";

    inverseCinematic(efectorGlobal);
}

void RobotHandler::inverseCinematic(const cv::Point3d& efectorGlobal) {
    // Implementación pendiente
    int R = sqrt(efectorGlobal.x * efectorGlobal.x + efectorGlobal.y * efectorGlobal.y);
    int Z = efectorGlobal.z;

    qDebug("Valores R y Z calculados: R = %d, Z = %d", R, Z);

    double B_rad = acos((R * R + (Z - a1 + a5) * (Z - a1 + a5) - a2 * a2 - a3 * a3) / (2 * a2 * a3) / 1000);
    int B = B_rad * 180 / M_PI;
    int A = 180 - B;

    qDebug("Angulos calculados: A = q2 = %d, B = q3 = %d", A, B);
}

// Transforma un punto del efector en coordenadas de la base
cv::Point3d RobotHandler::transformarPunto(const cv::Point3d& puntoLocal)
{
    // Crear punto homogéneo [x, y, z, 1]
    cv::Mat puntoHom = (cv::Mat_<double>(4, 1) << puntoLocal.x, puntoLocal.y, puntoLocal.z, 1);

    // Aplicar transformación total RTbt
    cv::Mat puntoGlobal = RTbt * puntoHom;

    // Devolver el punto transformado (coordenadas en la base del robot)
    return cv::Point3d(
        puntoGlobal.at<double>(0, 0),
        puntoGlobal.at<double>(1, 0),
        puntoGlobal.at<double>(2, 0)
    );
}

void RobotHandler::onDataSent(const QByteArray& data)
{
    if (m_serialConnected) {
        qDebug() << "[Serial] Data sent:" << QString::fromUtf8(data);
    }
}

RobotHandler::~RobotHandler() {}
