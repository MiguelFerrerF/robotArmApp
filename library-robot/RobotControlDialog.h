/**
 * @file RobotControlDialog.h
 * @author Miguel Ferrer
 * @brief   Dialog for manual control of the robot arm.
 * 
 * This class provides a GUI with sliders and text inputs to control the 6 motors
 * individually or in batches. It also allows setting calibration offsets and
 * defining specific positions (like "Place").
 * 
 * @version 0.1
 * @date 2026-01-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef ROBOTCONTROLDIALOG_H
#define ROBOTCONTROLDIALOG_H

#include "RobotConfig.h"
#include <QDialog>

namespace Ui
{
class RobotControlDialog;
}

/**
 * @brief Dialog for manual control of the robot arm.
 *
 * This class provides a GUI with sliders and text inputs to control the 6 motors
 * individually or in batches. It also allows setting calibration offsets and
 * defining specific positions (like "Place").
 */
class RobotControlDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RobotControlDialog(QWidget* parent = nullptr, RobotConfig::RobotSettings* settings = nullptr);
  ~RobotControlDialog();

  /**
   * @brief Loads the default angles from the settings into the UI sliders.
   * Effectively resets the visual controls to the "Home" position.
   */
  void setupOffsets();

signals:
  void errorOccurred(const QString& error);

  /**
   * @brief Emitted when a specific motor needs to move immediately.
   * Typically used when the UI is in "Single" mode.
   * @param motorIndex The 1-based index of the motor (1-6).
   * @param angle The target angle in degrees.
   */
  void motorAngleChanged(int motorIndex, int angle);

  /**
   * @brief Emitted to request a reset of all motors to their default positions.
   */
  void allMotorsReset();

  /**
   * @brief Emitted when the user redefines the "Home" offset for a motor.
   * @param motorIndex The 1-based index of the motor (1-6).
   * @param newOffset The new zero-point angle.
   */
  void motorOffsetChanged(int motorIndex, int newOffset);

  /**
   * @brief Emitted when the "Place" position is updated.
   * @param motorIndex The 1-based index of the motor.
   * @param position The angle stored for the placing sequence.
   */
  void placePositionChanged(int motorIndex, int position);

private slots:
  void on_pushButtonReset_clicked();
  void on_pushButtonSetup_clicked();
  void on_pushButtonAllSingle_clicked();
  void on_pushButtonSetOffsets_clicked();
  void on_pushButtonSetPlace_clicked();

private:
  Ui::RobotControlDialog*     ui;
  RobotConfig::RobotSettings* m_robotSettings;
  void                        connectSlidersToLineEdits();
  void                        connectLineEditsToSliders();
  void                        setLineEditToSliderValue(int motorIndex, int value);

  bool m_isAll = true;
};

#endif // ROBOTCONTROLDIALOG_H
