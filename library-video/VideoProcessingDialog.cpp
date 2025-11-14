#include "VideoProcessingDialog.h"
#include "./ui_VideoProcessingDialog.h"
#include "ClickableLabel.h"
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <opencv2/opencv.hpp>

VideoProcessingDialog::VideoProcessingDialog(QWidget* parent)
  : QDialog(parent), ui(new Ui::VideoProcessingDialog), m_selectedCorner(None), m_applySegmentacion(false)
{
  ui->setupUi(this);
  this->setWindowTitle("Camera Manager");
  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, &VideoProcessingDialog::handleNewPixmap);
  connect(&handler, &VideoCaptureHandler::propertiesSupported, this, &VideoProcessingDialog::on_propertiesSupported);
  connect(&handler, &VideoCaptureHandler::rangesSupported, this, &VideoProcessingDialog::on_rangesSupported);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this, &VideoProcessingDialog::on_cameraOpenFailed);

  connect(ui->videoLabel, &ClickableLabel::clickedAt, this, &VideoProcessingDialog::on_videoLabel_clicked);

  // Botones de selección de punto (Orden: TL, TR, BR, BL)
  connect(ui->ButtonpointTL, &QPushButton::clicked, this, [this]() {
    m_selectedCorner = TL;
    updatePointInfoLabel();
  });
  connect(ui->ButtonpointTR, &QPushButton::clicked, this, [this]() {
    m_selectedCorner = TR;
    updatePointInfoLabel();
  });
  connect(ui->ButtonpointBR, &QPushButton::clicked, this, [this]() {
    m_selectedCorner = BR;
    updatePointInfoLabel();
  });
  connect(ui->ButtonpointBL, &QPushButton::clicked, this, [this]() {
    m_selectedCorner = BL;
    updatePointInfoLabel();
  });

  // Llenar ComboBox de cámaras
  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  ui->comboBoxCameras->addItems(cameraNames);
  if (cameraNames.isEmpty()) {
    ui->startButton->setEnabled(false);
    ui->videoLabel->setText("No se han detectado cámaras.");
  }

  updateStartButtonState();
  setAllControlsEnabled(false);
}

VideoProcessingDialog::~VideoProcessingDialog()
{
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  delete ui;
}

// Actualiza estado Start/Stop
void VideoProcessingDialog::updateStartButtonState()
{
  bool isRunning = VideoCaptureHandler::instance().isCameraRunning();
  ui->startButton->setChecked(isRunning);
  ui->startButton->setText(isRunning ? "Stop" : "Start");
  ui->comboBoxCameras->setEnabled(!isRunning);
  ui->comboBoxResolution->setEnabled(!isRunning);

  if (isRunning)
    ui->videoLabel->setText("");
}

// Clic sobre la imagen para seleccionar puntos
void VideoProcessingDialog::on_videoLabel_clicked(const QPoint& pos)
{
  if (m_currentPixmap.isNull() || m_selectedCorner == None)
    return;

  QSize  pixSize = m_currentPixmap.size();
  QSize  lblSize = ui->videoLabel->size();
  double scale   = qMin(double(lblSize.width()) / pixSize.width(), double(lblSize.height()) / pixSize.height());

  int xOffset = (lblSize.width() - pixSize.width() * scale) / 2;
  int yOffset = (lblSize.height() - pixSize.height() * scale) / 2;

  QPointF scaled((pos.x() - xOffset) / scale, (pos.y() - yOffset) / scale);

  scaled.setX(qBound(0.0, scaled.x(), double(pixSize.width() - 1)));
  scaled.setY(qBound(0.0, scaled.y(), double(pixSize.height() - 1)));

  switch (m_selectedCorner) {
    case TL:
      m_cropPointTL = scaled.toPoint();
      break;
    case TR:
      m_cropPointTR = scaled.toPoint();
      break;
    case BR:
      m_cropPointBR = scaled.toPoint();
      break;
    case BL:
      m_cropPointBL = scaled.toPoint();
      break;
    default:
      break;
  }

  updatePointInfoLabel();
  drawCropPointsOnLabel();
}

void VideoProcessingDialog::updatePointInfoLabel()
{
  QString info;

  info += QString("TL: (%1, %2)\n").arg(m_cropPointTL.x()).arg(m_cropPointTL.y());
  info += QString("TR: (%1, %2)\n").arg(m_cropPointTR.x()).arg(m_cropPointTR.y());
  info += QString("BR: (%1, %2)\n").arg(m_cropPointBR.x()).arg(m_cropPointBR.y());
  info += QString("BL: (%1, %2)\n").arg(m_cropPointBL.x()).arg(m_cropPointBL.y());

  ui->labelCurrentPoint->setText(info);
}


// Dibujar puntos transformados sobre la imagen
void VideoProcessingDialog::drawCropPointsOnLabel()
{
  if (m_currentPixmap.isNull())
    return;

  QPixmap  annotated = m_currentPixmap;
  QPainter painter(&annotated);
  painter.setRenderHint(QPainter::Antialiasing);

  // Definir colores para cada punto y el orden: TL (1), TR (2), BR (3), BL (4)
  std::vector<QColor> colors = {Qt::red, Qt::green, Qt::blue, Qt::magenta};
  std::vector<QPoint> points = {m_cropPointTL, m_cropPointTR, m_cropPointBR, m_cropPointBL};

  // Dibujar el polígono que une los puntos
  bool allPointsDefined = true;
  for (const auto& pt : points) {
    if (pt == QPoint()) {
      allPointsDefined = false;
      break;
    }
  }

  if (allPointsDefined) {
    painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine)); // Color amarillo, línea discontinua
    painter.setBrush(Qt::NoBrush);
    QPolygon polygon;
    for (const auto& pt : points) {
      polygon << pt;
    }
    painter.drawPolygon(polygon);
  }

  // Dibujar cada punto con color y número
  for (size_t i = 0; i < points.size(); ++i) {
    const QPoint& pt = points[i];
    if (pt == QPoint())
      continue; // saltar si el punto no está definido

    painter.setPen(QPen(colors[i], 3));
    painter.setBrush(colors[i]);
    painter.drawEllipse(pt, 6, 6);

    // Dibujar número del punto
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(pt + QPoint(8, -8), QString::number(i + 1)); // número cerca del punto
  }

  painter.end();

  ui->videoLabel->setPixmap(annotated.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// Recibe nuevo pixmap de la cámara
void VideoProcessingDialog::handleNewPixmap(const QPixmap& pixmap)
{
  m_currentPixmap = pixmap;

  if (m_applySegmentacion && m_cropPointTL != QPoint() && m_cropPointTR != QPoint() && m_cropPointBL != QPoint() && m_cropPointBR != QPoint()) {
    // Recorte con perspectiva
    QPixmap cropped = applyPerspectiveCrop(m_currentPixmap, m_cropPointTL, m_cropPointTR, m_cropPointBR, m_cropPointBL, m_transformedCropPoints);

    // Aplicar segmentación sobre el crop
    applySegmentacion(cropped);

    // Mostrar resultado
    ui->videoLabel->setPixmap(cropped.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else {
    // Actualizar info al recibir frame
    updatePointInfoLabel();

    // Mostrar la imagen original + puntos predefinidos
    drawCropPointsOnLabel();
  }
}

// --- CORRECCIÓN DE PERSPECTIVA (Versión para Corregir Distorsión) ---
QPixmap VideoProcessingDialog::applyPerspectiveCrop(const QPixmap& original, const QPoint& tl, const QPoint& tr, const QPoint& br, const QPoint& bl,
                                                    std::vector<QPoint>& transformedPoints)
{
  if (original.isNull() || tl == QPoint() || tr == QPoint() || br == QPoint() || bl == QPoint())
    return original;

  QImage  img = original.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src(img.height(), img.width(), CV_8UC3, img.bits(), img.bytesPerLine());
  cv::Mat srcBGR;
  cv::cvtColor(src, srcBGR, cv::COLOR_RGB2BGR);

  std::vector<cv::Point2f> srcPts = {cv::Point2f(tl.x(), tl.y()), cv::Point2f(tr.x(), tr.y()), cv::Point2f(br.x(), br.y()),
                                     cv::Point2f(bl.x(), bl.y())};

  float widthTop    = cv::norm(srcPts[1] - srcPts[0]);
  float widthBottom = cv::norm(srcPts[2] - srcPts[3]);
  float heightLeft  = cv::norm(srcPts[3] - srcPts[0]);
  float heightRight = cv::norm(srcPts[2] - srcPts[1]);

  int W = std::max(widthTop, widthBottom);
  int H = std::max(heightLeft, heightRight);

  std::vector<cv::Point2f> dstPts = {cv::Point2f(0, 0), cv::Point2f(W - 1, 0), cv::Point2f(W - 1, H - 1), cv::Point2f(0, H - 1)};

  cv::Mat M = cv::getPerspectiveTransform(srcPts, dstPts);

  cv::Mat warped;
  cv::warpPerspective(srcBGR, warped, M, cv::Size(W, H));

  cv::Mat rgb;
  cv::cvtColor(warped, rgb, cv::COLOR_BGR2RGB);

  QImage out(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
  return QPixmap::fromImage(out);
}

// SEGMENTACIÓN
void VideoProcessingDialog::applySegmentacion(QPixmap& pixmap)
{
  if (pixmap.isNull())
    return;

  // Convertir QPixmap -> QImage -> cv::Mat (BGR)
  QImage  img_qt = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src_rgb(img_qt.height(), img_qt.width(), CV_8UC3, const_cast<uchar*>(img_qt.bits()), img_qt.bytesPerLine());
  cv::Mat image;
  cv::cvtColor(src_rgb, image, cv::COLOR_RGB2BGR); // OpenCV en BGR

  // Convertir a gris y detectar bordes
  cv::Mat gray, edges;
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
  cv::Canny(gray, edges, 50, 150);

  // Encontrar contornos
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edges.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  // Dibujar contornos y caja del más grande
  cv::Mat output      = image.clone();
  double  max_area    = 0;
  int     largest_idx = -1;

  for (size_t i = 0; i < contours.size(); i++) {
    double area = cv::contourArea(contours[i]);
    if (area < 100)
      continue; // descartar muy pequeños

    // Dibujar rectángulo verde y centro rojo
    cv::Rect box = cv::boundingRect(contours[i]);
    cv::rectangle(output, box, cv::Scalar(0, 255, 0), 2);

    cv::Moments M = cv::moments(contours[i]);
    if (M.m00 != 0) {
      int cx = int(M.m10 / M.m00);
      int cy = int(M.m01 / M.m00);
      cv::circle(output, cv::Point(cx, cy), 4, cv::Scalar(0, 0, 255), -1);
    }

    if (area > max_area) {
      max_area    = area;
      largest_idx = int(i);
    }
  }

  // Resaltar el contorno más grande
  if (largest_idx != -1) {
    cv::Rect largest_box = cv::boundingRect(contours[largest_idx]);
    cv::rectangle(output, largest_box, cv::Scalar(255, 0, 0), 3);
    cv::Moments M = cv::moments(contours[largest_idx]);
    if (M.m00 != 0) {
      int cx = int(M.m10 / M.m00);
      int cy = int(M.m01 / M.m00);
      cv::circle(output, cv::Point(cx, cy), 8, cv::Scalar(255, 255, 0), -1);
    }
  }

  // Convertir de nuevo a QPixmap
  cv::Mat output_rgb;
  cv::cvtColor(output, output_rgb, cv::COLOR_BGR2RGB);
  QImage outImg(output_rgb.data, output_rgb.cols, output_rgb.rows, output_rgb.step, QImage::Format_RGB888);
  pixmap = QPixmap::fromImage(outImg.copy()); // copia para asegurar memoria válida
}
// Actualizar label (Sin cambios)
void VideoProcessingDialog::updateVideoLabel()
{
  if (m_currentPixmap.isNull())
    return;
  ui->videoLabel->setPixmap(m_currentPixmap.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// --- Slots y funciones de cámara ---
void VideoProcessingDialog::on_checkBoxSegmentacion_toggled(bool checked)
{
  m_applySegmentacion = checked;

  // Si la cámara está corriendo, forzamos una actualización inmediata
  // de la visualización llamando a handleNewPixmap con la imagen actual.
  // Esto asegura que la imagen de la etiqueta cambie inmediatamente al estado correcto.
  if (VideoCaptureHandler::instance().isCameraRunning() && !m_currentPixmap.isNull()) {
    // Al llamar a handleNewPixmap, se procesa la m_currentPixmap.
    // Si 'checked' es true, se aplica crop+segmentación.
    // Si 'checked' es false, se muestra la imagen original con los puntos.
    handleNewPixmap(m_currentPixmap);
  }
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
    handler.requestCameraChange(-1, QSize());
    ui->startButton->setText("Start");
    ui->comboBoxCameras->setEnabled(true);
    ui->comboBoxResolution->setEnabled(true);
    m_currentPixmap = QPixmap();
    ui->videoLabel->clear();
    ui->videoLabel->setText("Cámara detenida.");
  }
}

void VideoProcessingDialog::on_resetButton_clicked()
{
  ui->checkBoxFocoAuto->setChecked(true);
  ui->checkBoxExposicionAuto->setChecked(true);
  ui->horizontalSliderBrillo->setValue(50);
  ui->horizontalSliderContraste->setValue(50);
  ui->horizontalSliderSaturacion->setValue(50);
  ui->horizontalSliderNitidez->setValue(50);
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
  ui->startButton->setChecked(false);
  ui->startButton->setText("Start OpenCV");
  ui->comboBoxCameras->setEnabled(true);
  ui->comboBoxResolution->setEnabled(true);
}

void VideoProcessingDialog::on_rangesSupported(const CameraPropertyRanges& ranges)
{
  m_ranges = ranges;
  ui->horizontalSliderBrillo->setValue(qBound(0, mapOpenCVToSlider(ranges.brightness.current, ranges.brightness), 100));
  ui->horizontalSliderContraste->setValue(qBound(0, mapOpenCVToSlider(ranges.contrast.current, ranges.contrast), 100));
  ui->horizontalSliderSaturacion->setValue(qBound(0, mapOpenCVToSlider(ranges.saturation.current, ranges.saturation), 100));
  ui->horizontalSliderNitidez->setValue(qBound(0, mapOpenCVToSlider(ranges.sharpness.current, ranges.sharpness), 100));
  ui->horizontalSliderExposicion->setValue(qBound(0, mapOpenCVToSlider(ranges.exposure.current, ranges.exposure), 100));
  ui->horizontalSliderFoco->setValue(qBound(0, mapOpenCVToSlider(ranges.focus.current, ranges.focus), 100));

  ui->checkBoxFocoAuto->setEnabled(m_support.autoFocus);
  ui->horizontalSliderBrillo->setEnabled(m_support.brightness);
  ui->horizontalSliderContraste->setEnabled(m_support.contrast);
  ui->horizontalSliderSaturacion->setEnabled(m_support.saturation);
  ui->horizontalSliderNitidez->setEnabled(m_support.sharpness);
  ui->checkBoxExposicionAuto->setEnabled(m_support.autoExposure);

  ui->horizontalSliderFoco->setEnabled(m_support.focus && !ui->checkBoxFocoAuto->isChecked());
  ui->horizontalSliderExposicion->setEnabled(m_support.exposure && !ui->checkBoxExposicionAuto->isChecked());
}

void VideoProcessingDialog::on_propertiesSupported(CameraPropertiesSupport support)
{
  m_support = support;
}

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

QSize VideoProcessingDialog::parseResolution(const QString& text)
{
  if (text == "Default")
    return QSize(0, 0);
  QStringList parts = text.split('x');
  if (parts.size() == 2) {
    bool ok1, ok2;
    int  w = parts[0].toInt(&ok1);
    int  h = parts[1].toInt(&ok2);
    if (ok1 && ok2)
      return QSize(w, h);
  }
  return QSize(0, 0);
}

int VideoProcessingDialog::mapSliderToOpenCV(int sliderValue, const PropertyRange& range)
{
  double outputRange = range.max - range.min;
  double mappedValue = range.min + sliderValue * outputRange / 100.0;
  return qBound(static_cast<int>(range.min), static_cast<int>(mappedValue), static_cast<int>(range.max));
}

int VideoProcessingDialog::mapOpenCVToSlider(double openCVValue, const PropertyRange& range)
{
  double inputRange = range.max - range.min;
  if (qFuzzyIsNull(inputRange))
    return 50;
  int sliderValue = static_cast<int>((openCVValue - range.min) / inputRange * 100.0);
  return qBound(0, sliderValue, 100);
}