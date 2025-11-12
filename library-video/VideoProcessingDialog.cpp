#include "VideoProcessingDialog.h"
#include "./ui_VideoProcessingDialog.h"
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMessageBox>
#include <QtMath>
#include <algorithm>          // Necesario para std::min/max
#include <opencv2/opencv.hpp> // NECESARIO para la corrección de perspectiva

VideoProcessingDialog::VideoProcessingDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoProcessingDialog)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Manager");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, &VideoProcessingDialog::handleNewPixmap);

  // Conexiones de soporte
  connect(&handler, &VideoCaptureHandler::propertiesSupported, this, &VideoProcessingDialog::on_propertiesSupported);
  connect(&handler, &VideoCaptureHandler::rangesSupported, this, &VideoProcessingDialog::on_rangesSupported);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this, &VideoProcessingDialog::on_cameraOpenFailed);

  // Rellenar ComboBox de cámaras
  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  ui->comboBoxCameras->addItems(cameraNames);

  if (cameraNames.isEmpty()) {
    ui->startButton->setEnabled(false);
    ui->videoLabel->setText("No se han detectado cámaras.");
  }

  // Si ya hay una cámara corriendo, cargamos su estado en la UI.
  updateStartButtonState();

  setAllControlsEnabled(false);
}

VideoProcessingDialog::~VideoProcessingDialog()
{
  // Desconectar la señal de newPixmapCaptured
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

// --- NUEVO: Actualiza el botón de Start/Stop al abrir el diálogo ---
void VideoProcessingDialog::updateStartButtonState()
{
  bool isRunning = VideoCaptureHandler::instance().isCameraRunning();
  ui->startButton->setChecked(isRunning);
  ui->startButton->setText(isRunning ? "Stop" : "Start");
  ui->comboBoxCameras->setEnabled(!isRunning);
  ui->comboBoxResolution->setEnabled(!isRunning);

  if (isRunning) {
    ui->videoLabel->setText(""); // Limpiar el texto si hay vídeo
  }
}

void VideoProcessingDialog::handleNewPixmap(const QPixmap& pixmap)
{
  m_currentPixmap       = pixmap;
  QPixmap workingPixmap = m_currentPixmap; // Usamos una copia para el procesamiento

  // --- 1. APLICACIÓN DE RECORTE NO RECTANGULAR/PERSPECTIVA CON 4 PUNTOS ---
  // Si la corrección de perspectiva está activa, se aplica primero
  if (m_applyPerspectiveCorrection) {
    workingPixmap = applyPerspectiveCrop(workingPixmap, m_cropPointTL, m_cropPointTR, m_cropPointBL, m_cropPointBR);
  }

  // --- 2. APLICACIÓN DE SEGMENTACIÓN ---
  // Ahora, la segmentación se aplica a la imagen *después* de la corrección de
  // perspectiva
  if (m_applySegmentacion) {
    applySegmentacion(workingPixmap); // applySegmentacion modifica el pixmap por referencia
  }

  // Actualizar la pixmap final que se mostrará en la UI
  m_currentPixmap = workingPixmap;
  updateVideoLabel();
}

/**
 * @brief Implementa la corrección de perspectiva utilizando OpenCV.
 * Convierte el cuadrilátero definido por tl, tr, bl, br en un
 * rectángulo, haciendo que sean las NUEVAS esquinas de la imagen.
 */

void VideoProcessingDialog::applySegmentacion(QPixmap& pixmap)
{
  if (pixmap.isNull()) {
    cv::destroyWindow("1. Grises");
    cv::destroyWindow("2. Bordes Canny");
    cv::destroyWindow("3. Contornos Final");
    return;
  }

  // --- 1. Conversión de QPixmap (RGB) a cv::Mat (BGR) ---
  QImage  img_qt = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src_rgb(img_qt.height(), img_qt.width(), CV_8UC3, img_qt.scanLine(0));
  cv::Mat image;

  cv::cvtColor(src_rgb, image, cv::COLOR_RGB2BGR);
  cv::Mat image_to_draw = image.clone();

  // --- 2. Detección de Bordes con Canny ---
  cv::Mat gray, edges;

  // Convertir a escala de grises y desenfocar
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

  // ************* MOSTRAR IMAGEN INTERMEDIA: Grises *************
  cv::imshow("1. Grises", gray);

  // Aplicar el detector de bordes Canny
  cv::Canny(gray, edges, 50, 150);

  // ********** MOSTRAR IMAGEN INTERMEDIA: Bordes Canny **********
  cv::imshow("2. Bordes Canny", edges);

  // Asignar la imagen de bordes a 'thresh' para el siguiente paso
  // (findContours)
  cv::Mat thresh = edges;

  // --- 3. Encontrar Contornos ---
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(thresh.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  // --- 4. Análisis de Contornos y Dibujado (Anteriormente etiquetado como 5)
  // ---
  double max_area              = 0;
  int    largest_contour_index = -1;
  // ... (El resto del análisis, recuadrado y centroides)
  // ...

  for (size_t i = 0; i < contours.size(); i++) {
    double area = cv::contourArea(contours[i]);

    if (area < 100)
      continue; // Filtrar ruido pequeño

    // a) Recuadro
    cv::Rect bounding_box = cv::boundingRect(contours[i]);

    // b) Centroide
    cv::Moments M = cv::moments(contours[i]);
    if (M.m00 != 0) {
      int cx = (int)(M.m10 / M.m00);
      int cy = (int)(M.m01 / M.m00);

      // c) Detectar la pieza más grande
      if (area > max_area) {
        max_area              = area;
        largest_contour_index = i;
      }

      // Dibujar el recuadro (todos los objetos)
      cv::rectangle(image_to_draw, bounding_box, cv::Scalar(0, 255, 0),
                    2); // Verde
      // Marcar el centro
      cv::circle(image_to_draw, cv::Point(cx, cy), 4, cv::Scalar(0, 0, 255),
                 -1); // Rojo
    }
  }

  // --- 5. Resaltar la Pieza Más Grande (Anteriormente etiquetado como 6) ---
  if (largest_contour_index != -1) {
    cv::Rect largest_box = cv::boundingRect(contours[largest_contour_index]);
    // Dibujar recuadro más grueso (Azul)
    cv::rectangle(image_to_draw, largest_box, cv::Scalar(255, 0, 0), 3);

    cv::Moments M = cv::moments(contours[largest_contour_index]);
    if (M.m00 != 0) {
      int cx = (int)(M.m10 / M.m00);
      int cy = (int)(M.m01 / M.m00);
      // Centroide grande (Amarillo)
      cv::circle(image_to_draw, cv::Point(cx, cy), 8, cv::Scalar(255, 255, 0), -1);
    }
  }

  // ********** MOSTRAR IMAGEN INTERMEDIA: Contornos Final **********
  cv::imshow("3. Contornos Final", image_to_draw);
  cv::waitKey(1);

  // --- 6. Conversión de cv::Mat (BGR) a QPixmap (RGB) para la UI de Qt ---
  cv::cvtColor(image_to_draw, image_to_draw, cv::COLOR_BGR2RGB);

  QImage outputImage(image_to_draw.data, image_to_draw.cols, image_to_draw.rows, image_to_draw.step, QImage::Format_RGB888);

  pixmap = QPixmap::fromImage(outputImage);
}

void VideoProcessingDialog::on_checkBoxSegmentacion_toggled(bool checked)
{
  m_applySegmentacion = checked;
}

void VideoProcessingDialog::on_startButton_clicked()
{
  VideoCaptureHandler& handler = VideoCaptureHandler::instance();
  if (ui->startButton->isChecked()) {

    int     cameraId   = ui->comboBoxCameras->currentIndex();
    QString resText    = ui->comboBoxResolution->currentText();
    QSize   resolution = parseResolution(resText);

    handler.requestCameraChange(cameraId, resolution);
    handler.setCameraName(ui->comboBoxCameras->currentText().toStdString());

    ui->startButton->setText("Stop");
    ui->comboBoxCameras->setEnabled(false);
    ui->comboBoxResolution->setEnabled(false);
  }
  else {
    // Estado: OFF (Detener)
    handler.requestCameraChange(-1, QSize());

    ui->startButton->setText("Start");
    ui->comboBoxCameras->setEnabled(true);
    ui->comboBoxResolution->setEnabled(true);

    m_currentPixmap = QPixmap();
    ui->videoLabel->clear();
    ui->videoLabel->setText("Cámara detenida.");
  }
}

QPixmap VideoProcessingDialog::applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& bl, const QPoint& br)
{
  if (original.isNull()) {
    return QPixmap();
  }

  // --- 1. Conversión QPixmap (RGB) a cv::Mat (BGR) ---
  QImage img = original.toImage();

  if (img.format() != QImage::Format_RGB888) {
    img = img.convertToFormat(QImage::Format_RGB888);
  }

  cv::Mat src_rgb(img.height(), img.width(), CV_8UC3, img.scanLine(0));
  cv::Mat src_bgr;
  cv::cvtColor(src_rgb, src_bgr, cv::COLOR_RGB2BGR); // Qt (RGB) -> OpenCV (BGR)

  // --- 2. Definir Puntos de Origen y Destino ---
  std::vector<cv::Point2f> srcPoints = {cv::Point2f(tl.x(), tl.y()), cv::Point2f(tr.x(), tr.y()), cv::Point2f(br.x(), br.y()),
                                        cv::Point2f(bl.x(), bl.y())};

  // Calcular dimensiones para el rectángulo de destino
  double topWidth    = qSqrt(qPow(tr.x() - tl.x(), 2) + qPow(tr.y() - tl.y(), 2));
  double bottomWidth = qSqrt(qPow(br.x() - bl.x(), 2) + qPow(br.y() - bl.y(), 2));
  int    maxWidth    = std::max(static_cast<int>(topWidth), static_cast<int>(bottomWidth));

  double leftHeight  = qSqrt(qPow(bl.x() - tl.x(), 2) + qPow(bl.y() - tl.y(), 2));
  double rightHeight = qSqrt(qPow(br.x() - tr.x(), 2) + qPow(br.y() - tr.y(), 2));
  int    maxHeight   = std::max(static_cast<int>(leftHeight), static_cast<int>(rightHeight));

  if (maxWidth <= 0 || maxHeight <= 0) {
    return original;
  }

  // Puntos de Destino (Rectángulo perfecto)
  std::vector<cv::Point2f> dstPoints = {cv::Point2f(0, 0), cv::Point2f(maxWidth - 1, 0), cv::Point2f(maxWidth - 1, maxHeight - 1),
                                        cv::Point2f(0, maxHeight - 1)};

  // --- 3. Aplicar Transformación ---
  cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
  cv::Mat warpedImage_bgr;
  cv::warpPerspective(src_bgr, warpedImage_bgr, perspectiveMatrix, cv::Size(maxWidth, maxHeight));

  // --- 4. Conversión CV::MAT (BGR) a QPixmap (RGB) ---
  cv::Mat warpedImage_rgb;
  cv::cvtColor(warpedImage_bgr, warpedImage_rgb,
               cv::COLOR_BGR2RGB); // OpenCV (BGR) -> Qt (RGB)

  QImage outputImage(warpedImage_rgb.data, warpedImage_rgb.cols, warpedImage_rgb.rows, warpedImage_rgb.step, QImage::Format_RGB888);

  return QPixmap::fromImage(outputImage);
}

void VideoProcessingDialog::on_resetButton_clicked()
{
  ui->checkBoxFocoAuto->setChecked(true);
  ui->checkBoxExposicionAuto->setChecked(true);
  ui->horizontalSliderBrillo->setValue(50);
  ui->horizontalSliderContraste->setValue(50);
  ui->horizontalSliderSaturacion->setValue(50);
  ui->horizontalSliderNitidez->setValue(50);
  // Foco y Exposición automáticos
  on_checkBoxFocoAuto_toggled(true);
  on_checkBoxExposicionAuto_toggled(true);
  on_horizontalSliderBrillo_sliderMoved(50);
  on_horizontalSliderContraste_sliderMoved(50);
  on_horizontalSliderSaturacion_sliderMoved(50);
  on_horizontalSliderNitidez_sliderMoved(50);
}

void VideoProcessingDialog::on_cameraOpenFailed(int cameraId, const QString& errorMsg)
{
  Q_UNUSED(cameraId);
  QMessageBox::critical(this, "Error de Cámara", tr("No se pudo iniciar la cámara seleccionada. Detalle: %1").arg(errorMsg));

  // Resetear el botón de inicio/parada y habilitar la selección de cámara
  ui->startButton->setChecked(false);
  ui->startButton->setText("Start OpenCV");
  ui->comboBoxCameras->setEnabled(true);
  ui->comboBoxResolution->setEnabled(true);
}

void VideoProcessingDialog::on_rangesSupported(const CameraPropertyRanges& ranges)
{
  m_ranges = ranges;

  // Brillo
  ui->horizontalSliderBrillo->setValue(qBound(0, mapOpenCVToSlider(ranges.brightness.current, ranges.brightness), 100));

  // Contraste
  ui->horizontalSliderContraste->setValue(qBound(0, mapOpenCVToSlider(ranges.contrast.current, ranges.contrast), 100));

  // Saturación
  ui->horizontalSliderSaturacion->setValue(qBound(0, mapOpenCVToSlider(ranges.saturation.current, ranges.saturation), 100));

  // Nitidez
  ui->horizontalSliderNitidez->setValue(qBound(0, mapOpenCVToSlider(ranges.sharpness.current, ranges.sharpness), 100));

  // Exposición
  ui->horizontalSliderExposicion->setValue(qBound(0, mapOpenCVToSlider(ranges.exposure.current, ranges.exposure), 100));

  // Foco
  ui->horizontalSliderFoco->setValue(qBound(0, mapOpenCVToSlider(ranges.focus.current, ranges.focus), 100));

  // Habilitar o deshabilitar controles según el soporte
  ui->checkBoxFocoAuto->setEnabled(m_support.autoFocus);
  ui->horizontalSliderBrillo->setEnabled(m_support.brightness);
  ui->horizontalSliderContraste->setEnabled(m_support.contrast);
  ui->horizontalSliderSaturacion->setEnabled(m_support.saturation);
  ui->horizontalSliderNitidez->setEnabled(m_support.sharpness);
  ui->checkBoxExposicionAuto->setEnabled(m_support.autoExposure);

  ui->horizontalSliderFoco->setEnabled(m_support.focus && !ui->checkBoxFocoAuto->isChecked());
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !ui->checkBoxExposicionAuto->isChecked());
}
// nuevo slot)
void VideoProcessingDialog::on_propertiesSupported(CameraPropertiesSupport support)
{
  m_support = support;
}

// Slots de Foco
void VideoProcessingDialog::on_checkBoxFocoAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoFocus(checked);
  ui->horizontalSliderFoco->setEnabled(m_support.focus && !checked);
}

void VideoProcessingDialog::on_checkBoxExposicionAuto_toggled(bool checked)
{
  VideoCaptureHandler::instance().setAutoExposure(checked);
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !checked);
}

void VideoProcessingDialog::on_horizontalSliderFoco_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.focus);
  VideoCaptureHandler::instance().setFocus(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderBrillo_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.brightness);
  VideoCaptureHandler::instance().setBrightness(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderContraste_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.contrast);
  VideoCaptureHandler::instance().setContrast(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderSaturacion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.saturation);
  VideoCaptureHandler::instance().setSaturation(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderNitidez_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.sharpness);
  VideoCaptureHandler::instance().setSharpness(openCVValue);
}

void VideoProcessingDialog::on_horizontalSliderExposicion_sliderMoved(int value)
{
  int openCVValue = mapSliderToOpenCV(value, m_ranges.exposure);
  VideoCaptureHandler::instance().setExposure(openCVValue);
}

void VideoProcessingDialog::setAllControlsEnabled(bool enabled)
{
  ui->checkBoxFocoAuto->setEnabled(enabled);
  ui->horizontalSliderFoco->setEnabled(enabled);
  ui->horizontalSliderBrillo->setEnabled(enabled);
  ui->horizontalSliderContraste->setEnabled(enabled);
  ui->horizontalSliderSaturacion->setEnabled(enabled);
  ui->horizontalSliderNitidez->setEnabled(enabled);
  ui->checkBoxExposicionAuto->setEnabled(enabled);
  ui->horizontalSliderExposicion->setEnabled(enabled);

  if (!enabled) {
    ui->checkBoxExposicionAuto->setChecked(true);
    ui->checkBoxFocoAuto->setChecked(true);
  }
}

void VideoProcessingDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull()) {
    return;
  }
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QSize VideoProcessingDialog::parseResolution(const QString& text)
{
  if (text == "Default") {
    return QSize(0, 0);
  }
  QStringList parts = text.split('x');
  if (parts.size() == 2) {
    bool ok1, ok2;
    int  w = parts[0].toInt(&ok1);
    int  h = parts[1].toInt(&ok2);
    if (ok1 && ok2) {
      return QSize(w, h);
    }
  }
  return QSize(0, 0);
}

int VideoProcessingDialog::mapSliderToOpenCV(int sliderValue, const PropertyRange& range)
{
  // Escala de [0, 100] (slider) a [range.min, range.max] (OpenCV)
  double outputRange = range.max - range.min;
  double scaleFactor = outputRange / 100.0;

  // Mapeo lineal: (ValorSlider * FactorEscala) + Mínimo
  double mappedValue = (sliderValue * scaleFactor) + range.min;

  // Asegurar que el valor se mantiene dentro del rango de OpenCV
  return qBound(static_cast<int>(range.min), static_cast<int>(mappedValue), static_cast<int>(range.max));
}

int VideoProcessingDialog::mapOpenCVToSlider(double openCVValue, const PropertyRange& range)
{
  // Escala de [range.min, range.max] (OpenCV) a [0, 100] (slider)
  double inputValue = openCVValue - range.min;
  double inputRange = range.max - range.min;

  if (qFuzzyIsNull(inputRange)) {
    return 50; // Evitar división por cero, devolver centro por defecto
  }

  double normalizedValue = inputValue / inputRange;

  // Mapeo al rango de 0 a 100
  int sliderValue = static_cast<int>(normalizedValue * 100.0);

  // Asegurar que el valor se mantiene dentro del rango del slider
  return qBound(0, sliderValue, 100);
}
