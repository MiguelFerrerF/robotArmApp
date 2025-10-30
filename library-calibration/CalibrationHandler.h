#ifndef CALIBRATION_HANDLER_H
#define CALIBRATION_HANDLER_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class CalibrationHandler {
public:
    CalibrationHandler(cv::Size boardSize = cv::Size (9, 6), float squareSize = 10.0f);
    void addImage(const cv::Mat& image);
    bool runCalibration();
    // Guarda la matriz de c�mara y los coeficientes de distorsi�n en archivos separados
    void saveCalibration(const std::string& cameraMatrixFile,
        const std::string& distCoeffsFile) const;
    bool loadCalibration(const std::string& filename);

    cv::Mat getCameraMatrix() const { return cameraMatrix; }
    cv::Mat getDistCoeffs() const { return distCoeffs; }
    cv::Mat getRotationVector() const { return rvec; }
    cv::Mat getTranslationVector() const { return tvec; }

private:
    cv::Size boardSize;        // Tama�o del tablero (n� de esquinas interiores)
    float squareSize;          // Tama�o real de cada cuadrado
    std::vector<std::vector<cv::Point2f>> imagePoints;
    std::vector<std::vector<cv::Point3f>> objectPoints;

    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    cv::Mat rvec, tvec;        // Rotaci�n y traslaci�n de la c�mara (extr�nsecos)

    std::vector<cv::Point3f> createObjectPoints() const;
};

#endif
