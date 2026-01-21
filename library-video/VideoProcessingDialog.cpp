/**
 * @file VideoProcessingDialog.cpp
 * @author Miguel Ferrer
 * @brief  Dialog for Video Processing and Object Segmentation.
 *
 * This file implements the VideoProcessingDialog class, which provides a user interface
 * for processing video frames captured from a camera. It allows users to define a region
 * of interest (ROI) by selecting corner points, applies perspective correction,
 * segments the object using Canny edge detection, and calculates key features such as
 * the centroid and orientation angle of the detected object. The dialog emits signals
 * to communicate updates to other parts of the application.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
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

/**
 * @brief Constructor.
 * Loads the saved crop coordinates (ROI) from QSettings to restore the previous session's state.
 * Connects to the VideoCaptureHandler to begin receiving frames immediately.
 */
VideoProcessingDialog::VideoProcessingDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoProcessingDialog), m_selectedCorner(None)
{
  ui->setupUi(this);
  this->setWindowTitle("Processing Video");
  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  // Load persistent ROI points
  QSettings settings("Empresa", "VideoProcessingApp");

  m_cropPointTL = settings.value("crop/TL", QPoint(175, 83)).toPoint();
  m_cropPointTR = settings.value("crop/TR", QPoint(423, 81)).toPoint();
  m_cropPointBR = settings.value("crop/BR", QPoint(484, 285)).toPoint();
  m_cropPointBL = settings.value("crop/BL", QPoint(131, 302)).toPoint();
  updatePointInfoLabel();

  VideoCaptureHandler& handler = VideoCaptureHandler::instance();

  connect(&handler, &VideoCaptureHandler::newPixmapCaptured, this, &VideoProcessingDialog::handleNewPixmap);
  connect(ui->videoLabel, &ClickableLabel::clickedAt, this, &VideoProcessingDialog::on_videoLabel_clicked);

  QStringList cameraNames;
  for (const QCameraDevice& camera : QMediaDevices::videoInputs()) {
    cameraNames << camera.description();
  }
  if (cameraNames.isEmpty()) {
    ui->videoLabel->setText("No se han detectado cámaras.");
  }
}

/**
 * @brief Destructor.
 * Saves the current crop coordinates (ROI) to QSettings for persistence across sessions.
 * Disconnects from the VideoCaptureHandler to stop receiving frames.
 */
VideoProcessingDialog::~VideoProcessingDialog()
{
  QSettings settings("Empresa", "VideoProcessingApp");
  settings.setValue("crop/TL", m_cropPointTL);
  settings.setValue("crop/TR", m_cropPointTR);
  settings.setValue("crop/BR", m_cropPointBR);
  settings.setValue("crop/BL", m_cropPointBL);

  disconnect(&VideoCaptureHandler::instance(), SIGNAL(newPixmapCaptured(QPixmap)), this, nullptr);
  disconnect(&VideoCaptureHandler::instance(), SIGNAL(cameraOpenFailed(int, QString)), this, nullptr);
  disconnect(ui->videoLabel, SIGNAL(clickedAt(QPoint)), this, nullptr);
  delete ui;
}

/**
 * @brief Slot triggered when the user clicks on the video label.
 * Maps the click position to the original image coordinates and updates the selected crop point.
 *
 * @param pos The position of the click within the label.
 */
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

  QSettings settings("Empresa", "VideoProcessingApp");
  switch (m_selectedCorner) {
    case TL:
      m_cropPointTL = scaled.toPoint();
      settings.setValue("crop/TL", m_cropPointTL);
      break;
    case TR:
      m_cropPointTR = scaled.toPoint();
      settings.setValue("crop/TR", m_cropPointTR);
      break;
    case BR:
      m_cropPointBR = scaled.toPoint();
      settings.setValue("crop/BR", m_cropPointBR);
      break;
    case BL:
      m_cropPointBL = scaled.toPoint();
      settings.setValue("crop/BL", m_cropPointBL);
      break;
    default:
      break;
  }

  updatePointInfoLabel();
  drawCropPointsOnLabel();
}

/**
 * @brief Updates the label displaying the current crop point coordinates.
 */
void VideoProcessingDialog::updatePointInfoLabel()
{
  QString info;

  info += QString("TL: (%1, %2)\n").arg(m_cropPointTL.x()).arg(m_cropPointTL.y());
  info += QString("TR: (%1, %2)\n").arg(m_cropPointTR.x()).arg(m_cropPointTR.y());
  info += QString("BR: (%1, %2)\n").arg(m_cropPointBR.x()).arg(m_cropPointBR.y());
  info += QString("BL: (%1, %2)\n").arg(m_cropPointBL.x()).arg(m_cropPointBL.y());

  ui->labelCurrentPoints->setText(info);
}

/**
 * @brief Draws the crop points and segmentation points on the video label.
 * Displays the crop points (TL, TR, BR, BL) with a polygon and numbers,
 * as well as the last segmentation points (centroid in red and line point in blue).
 *
 * Emits the processed image with annotations.
 */
void VideoProcessingDialog::drawCropPointsOnLabel()
{
  if (m_currentPixmap.isNull())
    return;

  QPixmap  annotated = m_currentPixmap;
  QPainter painter(&annotated);
  painter.setRenderHint(QPainter::Antialiasing);

  std::vector<QPoint> points = {m_cropPointTL, m_cropPointTR, m_cropPointBR, m_cropPointBL};

  bool allPointsDefined = true;
  for (const auto& pt : points) {
    if (pt == QPoint()) {
      allPointsDefined = false;
      break;
    }
  }
  if (allPointsDefined) {
    painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    QPolygon polygon;
    for (const auto& pt : points) {
      polygon << pt;
    }
    painter.drawPolygon(polygon);
  }

  for (size_t i = 0; i < points.size(); ++i) {
    const QPoint& pt = points[i];
    if (pt == QPoint())
      continue;
    painter.setPen(QPen(Qt::green, 3));
    painter.setBrush(Qt::green);
    painter.drawEllipse(pt, 6, 6);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(pt + QPoint(8, -8), QString::number(i + 1));
  }

  const int dotSize = 3;

  if (m_lastCentroidOriginal != QPoint()) {
    painter.setPen(QPen(Qt::red, dotSize / 2));
    painter.setBrush(Qt::red);
    painter.drawEllipse(m_lastCentroidOriginal, dotSize, dotSize);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(m_lastCentroidOriginal + QPoint(8, -8), "C");
  }

  if (m_lastPointRectaOriginal != QPoint()) {
    painter.setPen(QPen(Qt::blue, dotSize / 2));
    painter.setBrush(Qt::blue);
    painter.drawEllipse(m_lastPointRectaOriginal, dotSize, dotSize);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(m_lastPointRectaOriginal + QPoint(8, -8), "R");
  }

  painter.end();
  ui->videoLabel->setPixmap(annotated.scaled(ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  emit processedImageReady(annotated.toImage());
}

/**
 * @brief Main Pipeline Executor.
 *
 * This slot is the entry point for every video frame. It orchestrates:
 * 1. **Preprocessing:** Checks if ROI points are defined.
 * 2. **Perspective Warp:** Calls `applyPerspectiveCrop` to rectify the image.
 * 3. **Analysis:** Calls `applySegmentacion` to find objects in the rectified image.
 * 4. **Visualization:** Draws the ROI polygon and detected points on the UI.
 *
 * @param pixmap The raw frame captured by the camera thread.
 */
void VideoProcessingDialog::handleNewPixmap(const QPixmap& pixmap)
{
  m_currentPixmap = pixmap;

  if (m_cropPointTL != QPoint() && m_cropPointTR != QPoint() && m_cropPointBL != QPoint() && m_cropPointBR != QPoint()) {
    // 1. Warp Image
    QPixmap cropped = applyPerspectiveCrop(m_currentPixmap, m_cropPointTL, m_cropPointTR, m_cropPointBR, m_cropPointBL, m_transformedCropPoints);

    // 2. Detect Objects
    applySegmentacion(cropped);

    // 3. Update UI
    ui->labelCrop->setPixmap(cropped.scaled(ui->labelCrop->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else {
    // Reset points if ROI is incomplete
    updatePointInfoLabel();
  }
  drawCropPointsOnLabel();
}

/**
 * @brief Performs Perspective Transformation (Homography).
 *
 * Calculates the transformation matrix `M` that maps the user-defined quadrilateral (TL, TR, BR, BL)
 * to a rectangular destination image.
 *
 * @param original Input source image.
 * @param tl, tr, br, bl The 4 corners defining the ROI.
 * @return The rectified (top-down view) image.
 */
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

  // Calculate destination dimensions based on the maximum edge length
  float widthTop    = cv::norm(srcPts[1] - srcPts[0]);
  float widthBottom = cv::norm(srcPts[2] - srcPts[3]);
  float heightLeft  = cv::norm(srcPts[3] - srcPts[0]);
  float heightRight = cv::norm(srcPts[2] - srcPts[1]);

  int W = std::max(widthTop, widthBottom);
  int H = std::max(heightLeft, heightRight);

  std::vector<cv::Point2f> dstPts = {cv::Point2f(0, 0), cv::Point2f(W - 1, 0), cv::Point2f(W - 1, H - 1), cv::Point2f(0, H - 1)};

  // Compute Homography Matrix
  cv::Mat M = cv::getPerspectiveTransform(srcPts, dstPts);

  // Cache the matrix for inverse transformation later
  m_lastPerspectiveMatrix = M.clone();

  // Apply Warp
  cv::Mat warped;
  cv::warpPerspective(srcBGR, warped, M, cv::Size(W, H));

  cv::Mat rgb;
  cv::cvtColor(warped, rgb, cv::COLOR_BGR2RGB);

  // Transform crop points to new perspective
  QImage out(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
  return QPixmap::fromImage(out);
}

/**
 * @brief Executes the Image Segmentation and Feature Extraction algorithm.
 *
 * **Pipeline Steps:**
 * 1. **Downscaling:** Resizes image to 50% for performance.
 * 2. **Preprocessing:** Converts to Grayscale and applies Gaussian Blur.
 * 3. **Edge Detection:** Uses Canny algorithm to find edges.
 * 4. **Morphology:** Dilates edges to close gaps.
 * 5. **Contour Finding:** Retrieves external contours.
 * 6. **Analysis:**
 * - Filters contours by Area (> 300).
 * - Computes Image Moments (`cv::moments`).
 * - Calculates Centroid (Center of Mass).
 * - Calculates Orientation using `cv::minAreaRect` (Rotated Bounding Box).
 * 7. **Mapping:** Transforms calculated points back to Original Coordinates.
 *
 * @param[in,out] pixmap The cropped image (modified in-place with debug drawings).
 */
void VideoProcessingDialog::applySegmentacion(QPixmap& pixmap)
{
  if (pixmap.isNull())
    return;

  QImage  img_qt = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
  cv::Mat src_rgb(img_qt.height(), img_qt.width(), CV_8UC3, (uchar*)img_qt.bits(), img_qt.bytesPerLine());

  cv::Mat image_bgr;
  cv::cvtColor(src_rgb, image_bgr, cv::COLOR_RGB2BGR);

  cv::Mat small;
  cv::resize(image_bgr, small, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);

  float scale = 2.0f;

  cv::Mat gray, blurred_gray;
  cv::cvtColor(small, gray, cv::COLOR_BGR2GRAY);
  cv::blur(gray, blurred_gray, cv::Size(3, 3));

  // Edge Detection Pipeline

  // --- Canny ---
  cv::Mat edges;
  cv::Canny(blurred_gray, edges, 40, 100);

  // --- Dilation ---
  static cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::Mat        dilated_edges;
  cv::dilate(edges, dilated_edges, kernel);

  // --- Contours ---
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

    QImage cimg(filtered_edges_display.data, filtered_edges_display.cols, filtered_edges_display.rows, filtered_edges_display.step,
                QImage::Format_Grayscale8);
    ui->labelCanny->setPixmap(QPixmap::fromImage(cimg).scaled(ui->labelCanny->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    cv::Rect r = cv::boundingRect(c);
    cv::rectangle(output, r, cv::Scalar(255, 0, 0), 2);

    // Moment Calculation
    cv::Moments M = cv::moments(c);
    if (M.m00 > 0) {
      int cx = int(M.m10 / M.m00);
      int cy = int(M.m01 / M.m00);

      cv::Point centroidSmall(cx, cy);

      cv::circle(output, centroidSmall, 4, {0, 0, 255}, -1);
      centroid_str = QString("Centroide img recortada: (%1, %2)").arg(cx * scale).arg(cy * scale);

      if (c.size() >= 5) {

        // Orientation Calculation
        cv::RotatedRect rr    = cv::minAreaRect(c);
        double          angle = rr.angle;
        if (rr.size.width < rr.size.height)
          angle += 90;
        if (angle < 0)
          angle += 180;

        angle_str = QString::number(angle, 'f', 2) + "°";
        emit angleUpdated(angle + 90);

        double rad = angle * CV_PI / 180;

        double L_line  = 1.2 * std::sqrt(r.width * r.width + r.height * r.height);
        double L_point = 0.5 * L_line;

        cv::Point p1(cx + L_line * cos(rad), cy + L_line * sin(rad));
        cv::Point p2(cx - L_line * cos(rad), cy - L_line * sin(rad));
        cv::line(output, p1, p2, {0, 255, 255}, 2);
        cv::Point p_point_small(cx + L_point * cos(rad), cy + L_point * sin(rad));
        cv::circle(output, p_point_small, 4, {255, 255, 0}, -1);

        // --- CRITICAL: Coordinate Transformation ---
        // We found the point in the "Cropped" image. We must map it back to the "Original"
        // image so the robot knows where to go in the real physical space.

        // 1. Scale up (since we processed at 0.5x)
        cv::Point2f centroidCrop(centroidSmall.x * scale, centroidSmall.y * scale);
        cv::Point2f pointRectaCrop(p_point_small.x * scale, p_point_small.y * scale);

        // 2. Apply Inverse Perspective Transform
        QPoint centroidOriginal   = transformCropPointToOriginal(centroidCrop);
        QPoint pointRectaOriginal = transformCropPointToOriginal(pointRectaCrop);

        // Update persistent storage
        m_lastCentroidOriginal   = centroidOriginal;
        m_lastPointRectaOriginal = pointRectaOriginal;

        point_str = QString("Punto Recta img recortada: (%1, %2)").arg(int(pointRectaCrop.x)).arg(int(pointRectaCrop.y));

        QString centroidOriginalStr = QString("Centroide img original: (%1, %2)").arg(centroidOriginal.x()).arg(centroidOriginal.y());
        QString pointOriginalStr    = QString("Punto Recta img original: (%1, %2)").arg(pointRectaOriginal.x()).arg(pointRectaOriginal.y());

        ui->labelPoints->setText(centroid_str + "\n" + point_str + "\n" + centroidOriginalStr + "\n" + pointOriginalStr);

        // Emit signal for RobotHandler
        emit piecePointsUpdated(m_lastCentroidOriginal, m_lastPointRectaOriginal);
      }
    }
  }
  else {
    QImage cimg(filtered_edges_display.data, filtered_edges_display.cols, filtered_edges_display.rows, filtered_edges_display.step,
                QImage::Format_Grayscale8);
    ui->labelCanny->setPixmap(QPixmap::fromImage(cimg).scaled(ui->labelCanny->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    ui->labelPoints->setText(centroid_str + "\n" + point_str);
    m_lastCentroidOriginal   = QPoint();
    m_lastPointRectaOriginal = QPoint();
    emit piecePointsUpdated(m_lastCentroidOriginal, m_lastPointRectaOriginal);
  }
  ui->labelAngle->setText(angle_str);

  cv::resize(output, output, cv::Size(), scale, scale);
  cv::Mat output_rgb;
  cv::cvtColor(output, output_rgb, cv::COLOR_BGR2RGB);

  QImage out(output_rgb.data, output_rgb.cols, output_rgb.rows, output_rgb.step, QImage::Format_RGB888);

  pixmap = QPixmap::fromImage(out.copy());
}

/**
 * @brief Maps a 2D point from the Cropped View back to the Original View.
 *
 * Uses the inverse of the Homography matrix ($M^{-1}$) calculated during the crop phase.
 *  *
 * @param cropPoint The point (x,y) in the rectified image coordinates.
 * @return The point (x,y) in the original camera image coordinates.
 */
QPoint VideoProcessingDialog::transformCropPointToOriginal(const cv::Point2f& cropPoint)
{
  if (m_lastPerspectiveMatrix.empty()) {
    return QPoint();
  }

  std::vector<cv::Point2f> ptsInCrop = {cropPoint};
  std::vector<cv::Point2f> ptsInOriginal;

  // 1. Invert the perspective matrix
  cv::Mat M_inv = m_lastPerspectiveMatrix.inv();

  // 2. Apply perspectiveTransform (handles the homogeneous division w = 1/z)
  cv::perspectiveTransform(ptsInCrop, ptsInOriginal, M_inv);

  if (!ptsInOriginal.empty()) {
    return QPoint(qRound(ptsInOriginal[0].x), qRound(ptsInOriginal[0].y));
  }

  return QPoint();
}

/**
 * @brief Slot for selecting the Bottom-Left crop point.
 */
void VideoProcessingDialog::on_ButtonpointBL_clicked()
{
  m_selectedCorner = BL;
  updatePointInfoLabel();
}

/**
 * @brief Slot for selecting the Bottom-Right crop point.
 */
void VideoProcessingDialog::on_ButtonpointBR_clicked()
{
  m_selectedCorner = BR;
  updatePointInfoLabel();
}

/**
 * @brief Slot for selecting the Top-Left crop point.
 */
void VideoProcessingDialog::on_ButtonpointTL_clicked()
{
  m_selectedCorner = TL;
  updatePointInfoLabel();
}

/**
 * @brief Slot for selecting the Top-Right crop point.
 */
void VideoProcessingDialog::on_ButtonpointTR_clicked()
{
  m_selectedCorner = TR;
  updatePointInfoLabel();
}