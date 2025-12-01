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

VideoProcessingDialog::VideoProcessingDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoProcessingDialog), m_selectedCorner(None)
{
  ui->setupUi(this);
  this->setWindowTitle("Processing Video");
  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  // Conexión para recibir nuevos pixmaps capturados
  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, &VideoProcessingDialog::handleNewPixmap);
  connect(ui->videoLabel, &ClickableLabel::clickedAt, this, &VideoProcessingDialog::on_videoLabel_clicked);

  // Llenar ComboBox de cámaras
  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  if (cameraNames.isEmpty()) {
    ui->videoLabel->setText("No se han detectado cámaras.");
  }
}

VideoProcessingDialog::~VideoProcessingDialog()
{
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(cameraOpenFailed(int, QString)), this, nullptr);
  disconnect(ui->videoLabel, SIGNAL(clickedAt(QPoint)), this, nullptr);
  delete ui;
}

// Click sobre la imagen para seleccionar puntos
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

// Actualizar etiqueta con coordenadas de puntos
void VideoProcessingDialog::updatePointInfoLabel()
{
  QString info;

  info += QString("TL: (%1, %2)\n").arg(m_cropPointTL.x()).arg(m_cropPointTL.y());
  info += QString("TR: (%1, %2)\n").arg(m_cropPointTR.x()).arg(m_cropPointTR.y());
  info += QString("BR: (%1, %2)\n").arg(m_cropPointBR.x()).arg(m_cropPointBR.y());
  info += QString("BL: (%1, %2)\n").arg(m_cropPointBL.x()).arg(m_cropPointBL.y());

  ui->labelCurrentPoints->setText(info);
}

// Dibujar puntos sobre la imagen
void VideoProcessingDialog::drawCropPointsOnLabel()
{
  if (m_currentPixmap.isNull())
    return;

  QPixmap  annotated = m_currentPixmap;
  QPainter painter(&annotated);
  painter.setRenderHint(QPainter::Antialiasing);

  // 1. DIBUJAR PUNTOS DE RECORTE (TL, TR, BR, BL) y el POLÍGONO

  // Definir para cada punto el orden: TL (1), TR (2), BR (3), BL (4)
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

    painter.setPen(QPen(Qt::green, 3));
    painter.setBrush(Qt::green);
    painter.drawEllipse(pt, 6, 6);

    // Dibujar número del punto
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(pt + QPoint(8, -8), QString::number(i + 1)); // número cerca del punto
  }

  // 2. DIBUJAR PUNTOS DE SEGMENTACIÓN PERSISTENTES (Centroide en Rojo y Punto de Recta en Azul)

  const int dotSize = 3; // Usaremos un tamaño un poco más grande para destacarlos

  // --- Dibujar el Centroide (ROJO) ---
  if (m_lastCentroidOriginal != QPoint()) {
    painter.setPen(QPen(Qt::red, dotSize / 2)); // Borde rojo más fino
    painter.setBrush(Qt::red);
    painter.drawEllipse(m_lastCentroidOriginal, dotSize, dotSize);

    // Opcional: etiquetar el punto
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(m_lastCentroidOriginal + QPoint(8, -8), "C");
  }

  // --- Dibujar el Punto de la Recta (AZUL) ---
  if (m_lastPointRectaOriginal != QPoint()) {
    painter.setPen(QPen(Qt::blue, dotSize / 2)); // Borde azul más fino
    painter.setBrush(Qt::blue);
    painter.drawEllipse(m_lastPointRectaOriginal, dotSize, dotSize);

    // Opcional: etiquetar el punto
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(m_lastPointRectaOriginal + QPoint(8, -8), "R");
  }

  painter.end();
  ui->videoLabel->setPixmap(annotated.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// Recibe nuevo pixmap de la cámara
void VideoProcessingDialog::handleNewPixmap(const QPixmap& pixmap)
{
  m_currentPixmap = pixmap;

  if (m_cropPointTL != QPoint() && m_cropPointTR != QPoint() && m_cropPointBL != QPoint() && m_cropPointBR != QPoint()) {
    // Recorte con perspectiva
    QPixmap cropped = applyPerspectiveCrop(m_currentPixmap, m_cropPointTL, m_cropPointTR, m_cropPointBR, m_cropPointBL, m_transformedCropPoints);

    // Aplicar segmentación sobre el crop
    applySegmentacion(cropped);

    // Mostrar resultado
    ui->labelCrop->setPixmap(cropped.scaled(ui->labelCrop->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else {
    // Actualizar info al recibir frame
    updatePointInfoLabel();
  }
  // Mostrar la imagen original + puntos predefinidos
  drawCropPointsOnLabel();
}

// CORRECCIÓN DE PERSPECTIVA
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

  // Guardar puntos transformados
  m_lastPerspectiveMatrix = M.clone();

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

  // --- Convertir QPixmap -> Mat rápido ---
  QImage  img_qt = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src_rgb(img_qt.height(), img_qt.width(), CV_8UC3, (uchar*)img_qt.bits(), img_qt.bytesPerLine());

  cv::Mat image_bgr;
  cv::cvtColor(src_rgb, image_bgr, cv::COLOR_RGB2BGR);

  cv::Mat small;
  cv::resize(image_bgr, small, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);

  float scale = 2.0f;

  // --- Gris + blur rápido ---
  cv::Mat gray, blurred_gray;
  cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
  cv::blur(gray, blurred_gray, cv::Size(3, 3));

  // --- Canny rápido ---
  cv::Mat edges;
  cv::Canny(blurred_gray, edges, 40, 100);

  // --- Dilatación (reusa kernel) ---
  static cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::Mat        dilated_edges;
  cv::dilate(edges, dilated_edges, kernel);

  // --- Contornos ---
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(dilated_edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  cv::Mat output      = small.clone();
  int     largest_idx = -1;
  double  max_area    = 0;

  for (int i = 0; i < (int)contours.size(); i++) {
    double a = cv::contourArea(contours[i]);
    if (a > 300.0 && a > max_area) {
      max_area    = a;
      largest_idx = i;
    }
  }

  QString centroid_str = "Centroide img recortada: N/A";
  QString point_str    = "Punto Recta img recortada: N/A";
  QString angle_str    = "N/A";

  cv::Mat filtered_edges_display = cv::Mat::zeros(edges.size(), edges.type());

  if (largest_idx != -1) {
    const auto& c = contours[largest_idx];
    cv::drawContours(filtered_edges_display, contours, largest_idx, 255, 1);

    // Mostrar Canny filtrado
    QImage cimg(filtered_edges_display.data, filtered_edges_display.cols, filtered_edges_display.rows, filtered_edges_display.step,
                QImage::Format_Grayscale8);
    ui->labelCanny->setPixmap(QPixmap::fromImage(cimg).scaled(ui->labelCanny->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Bounding box
    cv::Rect r = cv::boundingRect(c);
    cv::rectangle(output, r, cv::Scalar(255, 0, 0), 2);

    // Momento
    cv::Moments M = cv::moments(c);
    if (M.m00 > 0) {
      int cx = int(M.m10 / M.m00);
      int cy = int(M.m01 / M.m00);

      // Puntos en coordenadas del crop REDIMENSIONADO (small)
      cv::Point centroidSmall(cx, cy);

      cv::circle(output, centroidSmall, 4, {0, 0, 255}, -1);
      centroid_str = QString("Centroide img recortada: (%1, %2)").arg(cx * scale).arg(cy * scale);

      if (c.size() >= 5) {
        cv::RotatedRect rr    = cv::minAreaRect(c);
        double          angle = rr.angle;
        if (rr.size.width < rr.size.height)
          angle += 90;
        if (angle < 0)
          angle += 180;

        angle_str = QString::number(angle, 'f', 2) + "°";

        double rad = angle * CV_PI / 180;

        // --- Longitudes proporcionales al tamaño del rectángulo ---
        double L_line  = 1.2 * std::sqrt(r.width * r.width + r.height * r.height);
        double L_point = 0.5 * L_line;

        // --- Línea amarilla completa (en imagen *small*) ---
        cv::Point p1(cx + L_line * cos(rad), cy + L_line * sin(rad));
        cv::Point p2(cx - L_line * cos(rad), cy - L_line * sin(rad));
        cv::line(output, p1, p2, {0, 255, 255}, 2);

        // --- Punto azul-cielo (en imagen *small*) ---
        cv::Point p_point_small(cx + L_point * cos(rad), cy + L_point * sin(rad));
        cv::circle(output, p_point_small, 4, {255, 255, 0}, -1);

        // --- Transformación de Coordenadas Inversa (Crop -> Original) ---

        // Puntos en coordenadas del crop *sin escalar* (escala 1.0)
        cv::Point2f centroidCrop(centroidSmall.x * scale, centroidSmall.y * scale);
        cv::Point2f pointRectaCrop(p_point_small.x * scale, p_point_small.y * scale);

        // Obtener puntos en la imagen original
        QPoint centroidOriginal   = transformCropPointToOriginal(centroidCrop);
        QPoint pointRectaOriginal = transformCropPointToOriginal(pointRectaCrop);

        // *** AÑADIDO: ALMACENAR PUNTOS PARA QUE PERSISTAN ***
        m_lastCentroidOriginal   = centroidOriginal;
        m_lastPointRectaOriginal = pointRectaOriginal;
        // **************************************************

        point_str = QString("Punto Recta img recortada: (%1, %2)").arg(int(pointRectaCrop.x)).arg(int(pointRectaCrop.y));

        QString centroidOriginalStr = QString("Centroide img original: (%1, %2)").arg(centroidOriginal.x()).arg(centroidOriginal.y());
        QString pointOriginalStr    = QString("Punto Recta img original: (%1, %2)").arg(pointRectaOriginal.x()).arg(pointRectaOriginal.y());

        ui->labelPoints->setText(centroid_str + "\n" + point_str + "\n" + centroidOriginalStr + "\n" + pointOriginalStr);
      }
    }
  }

  ui->labelAngle->setText(angle_str);

  // --- Convert back (redimensionar output al tamaño original del crop) ---
  cv::resize(output, output, cv::Size(), scale, scale);

  cv::Mat output_rgb;
  cv::cvtColor(output, output_rgb, cv::COLOR_BGR2RGB);

  QImage out(output_rgb.data, output_rgb.cols, output_rgb.rows, output_rgb.step, QImage::Format_RGB888);

  pixmap = QPixmap::fromImage(out.copy());
}

// Transforma una coordenada (en cv::Point2f) del frame recortado a una coordenada (en QPoint) del frame original
QPoint VideoProcessingDialog::transformCropPointToOriginal(const cv::Point2f& cropPoint)
{
  if (m_lastPerspectiveMatrix.empty()) {
    // Devolver un punto no válido si no hay matriz
    return QPoint();
  }

  // El punto de entrada está en coordenadas de imagen (no escaladas)
  std::vector<cv::Point2f> ptsInCrop = {cropPoint};
  std::vector<cv::Point2f> ptsInOriginal;

  // 1. Invertir la matriz de perspectiva (la inversa mapea el destino al origen)
  cv::Mat M_inv = m_lastPerspectiveMatrix.inv();

  // 2. Aplicar la transformación inversa
  cv::perspectiveTransform(ptsInCrop, ptsInOriginal, M_inv);

  // 3. Devolver el punto transformado como QPoint (redondeando a entero)
  if (!ptsInOriginal.empty()) {
    return QPoint(qRound(ptsInOriginal[0].x), qRound(ptsInOriginal[0].y));
  }

  return QPoint();
}

// --- Slots y funciones de cámara ---
void VideoProcessingDialog::on_checkBoxSegmentacion_toggled(bool checked)
{
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