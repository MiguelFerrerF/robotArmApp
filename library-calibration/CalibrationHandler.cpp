#include "CalibrationHandler.h"
#include <iostream>
#include <fstream>
#include <filesystem> 
#include <QDebug>

namespace fs = std::filesystem;


CalibrationHandler::CalibrationHandler(cv::Size boardSize, float squareSize)
    : boardSize(boardSize), squareSize(squareSize) {
	qDebug() << "CalibrationHandler creado con boardSize:"
		<< boardSize.width << "x" << boardSize.height
		<< "y squareSize:" << squareSize;
}

std::vector<cv::Point3f> CalibrationHandler::createObjectPoints() const {
    std::vector<cv::Point3f> obj;
    for (int i = 0; i < boardSize.height; ++i) {
        for (int j = 0; j < boardSize.width; ++j) {
            obj.emplace_back(j * squareSize, i * squareSize, 0);
        }
    }
    return obj;
}

void CalibrationHandler::addImage(const cv::Mat& image) {
    std::vector<cv::Point2f> corners;
    bool found = cv::findChessboardCorners(image, boardSize, corners,
        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

    if (found) {
        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
            cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001));

        imagePoints.push_back(corners);
        objectPoints.push_back(createObjectPoints());

        std::cout << "Esquinas detectadas correctamente.\n";
    }
    else {
        std::cout << "No se detectaron esquinas en esta imagen.\n";
    }
}

bool CalibrationHandler::runCalibration() {
    if (imagePoints.size() < 5) {
        std::cerr << "Error: se necesitan al menos 5 imágenes válidas para calibrar.\n";
        return false;
    }

    std::vector<cv::Mat> rvecs, tvecs;
    double rms = cv::calibrateCamera(objectPoints, imagePoints, boardSize,
        cameraMatrix, distCoeffs, rvecs, tvecs);

    std::cout << "RMS error de calibración: " << rms << std::endl;
    std::cout << "Matriz de cámara:\n" << cameraMatrix << std::endl;
    std::cout << "Coeficientes de distorsión:\n" << distCoeffs << std::endl;

    // Tomar la primera extrínseca como referencia
    if (!rvecs.empty()) {
        rvec = rvecs[0];
        tvec = tvecs[0];
        std::cout << "Rotación:\n" << rvec << std::endl;
        std::cout << "Traslación:\n" << tvec << std::endl;
    }

    return true;
}

void CalibrationHandler::saveCalibration(const std::string& cameraMatrixFile,
    const std::string& distCoeffsFile) const
{
    // Crear carpeta "CalibrationResults" si no existe
    std::string folderName = "CalibrationResults";
    if (!fs::exists(folderName)) {
        fs::create_directory(folderName);
        std::cout << "Carpeta creada: " << folderName << std::endl;
    }

    // Rutas completas de los archivos
    std::string cameraMatrixPath = folderName + "/" + cameraMatrixFile;
    std::string distCoeffsPath = folderName + "/" + distCoeffsFile;

    // Guardar matriz de cámara
    cv::FileStorage fsCam(cameraMatrixPath, cv::FileStorage::WRITE);
    if (!fsCam.isOpened()) {
        std::cerr << "Error al abrir archivo para cameraMatrix: " << cameraMatrixPath << std::endl;
        return;
    }
    fsCam << "cameraMatrix" << cameraMatrix;
    fsCam.release();
    std::cout << "Matriz de cámara guardada en " << cameraMatrixPath << std::endl;

    // Guardar coeficientes de distorsión
    cv::FileStorage fsDist(distCoeffsPath, cv::FileStorage::WRITE);
    if (!fsDist.isOpened()) {
        std::cerr << "Error al abrir archivo para distCoeffs: " << distCoeffsPath << std::endl;
        return;
    }
    fsDist << "distCoeffs" << distCoeffs;
    fsDist.release();
    std::cout << "Coeficientes de distorsión guardados en " << distCoeffsPath << std::endl;
}


bool CalibrationHandler::loadCalibration(const std::string& filename) {
    cv::FileStorage fs(filename, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "Error al abrir el archivo de calibración.\n";
        return false;
    }

    fs["cameraMatrix"] >> cameraMatrix;
    fs["distCoeffs"] >> distCoeffs;
    fs["rvec"] >> rvec;
    fs["tvec"] >> tvec;
    fs.release();
    std::cout << "Calibración cargada desde " << filename << std::endl;
    return true;
}
