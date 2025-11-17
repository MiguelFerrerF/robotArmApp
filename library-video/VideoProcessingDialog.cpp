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
  this->setWindowTitle("Processing Video");
  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, &VideoProcessingDialog::handleNewPixmap);
  connect(&handler, &VideoCaptureHandler::cameraOpenFailed, this, &VideoProcessingDialog::on_cameraOpenFailed);

  connect(ui->videoLabel, &ClickableLabel::clickedAt, this, &VideoProcessingDialog::on_videoLabel_clicked);

  // Llenar ComboBox de cámaras
  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  if (cameraNames.isEmpty()) {
    ui->startButton->setEnabled(false);
    ui->videoLabel->setText("No se han detectado cámaras.");
  }

  updateStartButtonState();
}

VideoProcessingDialog::~VideoProcessingDialog()
{
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(cameraOpenFailed(int, QString)), this, nullptr);
  disconnect(ui->videoLabel, SIGNAL(clickedAt(QPoint)), this, nullptr);
  delete ui;
}

void VideoProcessingDialog::on_ButtonpointBL_clicked()
{
  m_selectedCorner = BL;
  updatePointInfoLabel();
}

void VideoProcessingDialog::on_ButtonpointBR_clicked()
{
  m_selectedCorner = BR;
  updatePointInfoLabel();
}

void VideoProcessingDialog::on_ButtonpointTL_clicked()
{
  m_selectedCorner = TL;
  updatePointInfoLabel();
}

void VideoProcessingDialog::on_ButtonpointTR_clicked()
{
  m_selectedCorner = TR;
  updatePointInfoLabel();
}


// Actualiza estado Start/Stop
void VideoProcessingDialog::updateStartButtonState()
{
  bool isRunning = VideoCaptureHandler::instance().isCameraRunning();
  ui->startButton->setChecked(isRunning);
  ui->startButton->setText(isRunning ? "Stop" : "Start");

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

  ui->labelCurrentPoints->setText(info);
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

  // 1. ---- Convertir QPixmap -> cv::Mat (BGR) ----
  QImage  img_qt = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src_rgb(img_qt.height(), img_qt.width(), CV_8UC3, const_cast<uchar*>(img_qt.bits()), img_qt.bytesPerLine());
  cv::Mat image_bgr;
  cv::cvtColor(src_rgb, image_bgr, cv::COLOR_RGB2BGR); // Convertimos a BGR para el estándar de OpenCV

  // 2. ---- Gris + Canny ----
  cv::Mat gray, blurred_gray, edges;
  cv::cvtColor(image_bgr, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, blurred_gray, cv::Size(5, 5), 0);

  QImage gray_qt(blurred_gray.data, blurred_gray.cols, blurred_gray.rows, blurred_gray.step, QImage::Format_Grayscale8);
  ui->labelGray->setPixmap(QPixmap::fromImage(gray_qt).scaled(ui->labelGray->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

  cv::Canny(blurred_gray, edges, 20, 50);

  // =========================================================
  // 2.A. ---- Cerrar Bordes con Dilatación ----
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::Mat dilated_edges;
  cv::dilate(edges, dilated_edges, kernel, cv::Point(-1, -1), 1);
  // =========================================================

  // 3. ---- Contornos ----
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(dilated_edges.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  cv::Mat output      = image_bgr.clone();
  double  max_area    = 0;
  int     largest_idx = -1;

  // --- Umbral de área mínima y filtrado ---
  const double                        MIN_CONTOUR_AREA = 500.0;
  std::vector<std::vector<cv::Point>> filtered_contours;

  for (const auto& contour : contours) {
    double area = cv::contourArea(contour);
    if (area > MIN_CONTOUR_AREA) {
      filtered_contours.push_back(contour);
    }
  }

  // === 3.A. Identificar el contorno más grande entre los filtrados ===
  for (size_t i = 0; i < filtered_contours.size(); i++) {
    double area = cv::contourArea(filtered_contours[i]);
    if (area > max_area) {
      max_area    = area;
      largest_idx = int(i);
    }
  }

  // === 3.B. Crear la imagen filtrada para labelCanny ===
  cv::Mat filtered_edges_display = cv::Mat::zeros(edges.size(), edges.type());

  // Inicializar strings de salida en caso de que no se encuentre la pieza
  QString centroid_str = "Centroide: N/A";
  QString point_str    = "Punto Recta: N/A";
  QString angle_str    = "Ángulo: N/A";

  if (largest_idx != -1) {
    const auto& main_contour = filtered_contours[largest_idx];

    // Dibuja SÓLO el contorno más grande y filtrado
    cv::drawContours(filtered_edges_display, filtered_contours, largest_idx, cv::Scalar(255), 1);

    // --- 3.C. MOSTRAR IMAGEN FILTRADA EN labelCanny ---
    QImage canny_qt(filtered_edges_display.data, filtered_edges_display.cols, filtered_edges_display.rows, filtered_edges_display.step,
                    QImage::Format_Grayscale8);
    ui->labelCanny->setPixmap(QPixmap::fromImage(canny_qt).scaled(ui->labelCanny->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 3.4. Rectángulo Delimitador (Bounding Box)
    cv::Rect bounding_rect = cv::boundingRect(main_contour);
    cv::rectangle(output, bounding_rect.tl(), bounding_rect.br(), cv::Scalar(255, 0, 0), 2); // Azul (BGR)

    // 3.5. Centroide (Momento de la imagen)
    cv::Moments M  = cv::moments(main_contour);
    int         cx = -1, cy = -1;

    if (M.m00 > 0) { // Evitar división por cero
      cx = static_cast<int>(M.m10 / M.m00);
      cy = static_cast<int>(M.m01 / M.m00);

      // Dibujar el Centroide en la imagen final (output)
      cv::circle(output, cv::Point(cx, cy), 5, cv::Scalar(0, 0, 255), -1); // Rojo (BGR)

      // Preparar string del Centroide
      centroid_str = QString("Centroide: (%1, %2)").arg(cx).arg(cy);
    }

    // 3.6. Orientación y Línea Principal
    if (main_contour.size() >= 5) {
      cv::RotatedRect min_rect = cv::minAreaRect(main_contour);
      double          angle    = min_rect.angle;

      if (min_rect.size.width < min_rect.size.height) {
        angle = angle + 90.0;
      }
      if (angle < 0)
        angle += 180.0;

      // Preparar string del Ángulo
      angle_str = QString::number(angle, 'f', 2) + "°";

      // Dibujar el eje principal (línea de orientación)
      if (cx != -1 && cy != -1) {
        double rad = angle * CV_PI / 180.0;
        // Usamos una longitud suficiente para que la línea intercepte la pieza
        int line_length = 200;

        // Calcular el punto final P1 (el punto que intercepta el rectangulo)
        int x1 = cx + static_cast<int>(line_length * cos(rad));
        int y1 = cy + static_cast<int>(line_length * sin(rad));

        // Calcular el punto final P2 (el punto opuesto)
        int x2 = cx - static_cast<int>(line_length * cos(rad));
        int y2 = cy - static_cast<int>(line_length * sin(rad));

        // Dibujar la línea de orientación
        cv::line(output, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 255), 2); // Amarillo (BGR)

        // Preparar string del Punto de la Recta (usando P1)
        point_str = QString("Punto Recta: (%1, %2)").arg(x1).arg(y1);

        // Dibujar el punto de la recta en la imagen para confirmación (Opcional)
        cv::circle(output, cv::Point(x1, y1), 5, cv::Scalar(255, 255, 0), -1); // Cian (BGR)
      }
    }
  }
  // --- ACTUALIZACIÓN DE LABELS DE SALIDA (Fuera o dentro del if/else según prefieras el N/A) ---

  // Actualizar labelPuntos con el Centroide y el Punto de la Recta
  if (ui->labelPoints) {
    ui->labelPoints->setText(centroid_str + "\n" + point_str);
  }

  // Actualizar labelAngle con el ángulo
  if (ui->labelAngle) {
    ui->labelAngle->setText(angle_str);
  }

  if (largest_idx == -1) {
    // Si no se encuentra un contorno grande, muestra la imagen negra vacía en Canny
    QImage canny_qt(filtered_edges_display.data, filtered_edges_display.cols, filtered_edges_display.rows, filtered_edges_display.step,
                    QImage::Format_Grayscale8);
    ui->labelCanny->setPixmap(QPixmap::fromImage(canny_qt).scaled(ui->labelCanny->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  // 4. ---- Convertir cv::Mat (BGR) -> QPixmap ----
  cv::Mat output_rgb;
  cv::cvtColor(output, output_rgb, cv::COLOR_BGR2RGB);

  QImage outImg(output_rgb.data, output_rgb.cols, output_rgb.rows, output_rgb.step, QImage::Format_RGB888);
  pixmap = QPixmap::fromImage(outImg.copy());
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
    ui->startButton->setText("Stop");
  }
  else {
    handler.requestCameraChange(-1, QSize());
    ui->startButton->setText("Start");
    m_currentPixmap = QPixmap();
    ui->videoLabel->clear();
    ui->videoLabel->setText("Cámara detenida.");
  }
}


void VideoProcessingDialog::on_cameraOpenFailed(int cameraId, const QString& errorMsg)
{
  Q_UNUSED(cameraId);
  QMessageBox::critical(this, "Error de Cámara", tr("No se pudo iniciar la cámara seleccionada. Detalle: %1").arg(errorMsg));
  ui->startButton->setChecked(false);
  ui->startButton->setText("Start OpenCV");
}