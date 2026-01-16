#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QIntValidator>
#include <QPixmap>
#include <QSlider>

RobotControlDialog::RobotControlDialog(QWidget* parent, RobotConfig::RobotSettings* settings)
  : QDialog(parent), ui(new Ui::RobotControlDialog), m_robotSettings(settings)
{
  ui->setupUi(this);
  this->setWindowTitle("Robot Control");

  this->setWindowFlags(this->windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

  // Imagen principal del robot (se mantiene el QLabel de la imagen)
  QPixmap pixmap(QCoreApplication::applicationDirPath() + "/images/robot.png");
  ui->labelRobotImage->setAlignment(Qt::AlignCenter);
  ui->labelRobotImage->setScaledContents(true);
  ui->labelRobotImage->setPixmap(pixmap.scaled(500, 500, Qt::KeepAspectRatio));

  connectSlidersToLineEdits();
  connectLineEditsToSliders();

  on_pushButtonReset_clicked();
}

RobotControlDialog::~RobotControlDialog()
{
  delete ui;
}

/**
 * @brief Establishes two-way binding between Sliders and LineEdits.
 *
 * Connects the `valueChanged` signal of the QSliders to update the text
 * fields and the internal data model. Uses a lambda to prevent recursion loops.
 */
void RobotControlDialog::connectSlidersToLineEdits()
{
  QSlider* sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2, ui->horizontalSlider3,
                        ui->horizontalSlider4, ui->horizontalSlider5, ui->horizontalSlider6};
  for (int i = 0; i < 6; ++i) {
    connect(sliders[i], &QSlider::valueChanged, this, [this, i](int value) {
      static bool updating = false;
      if (updating)
        return;
      updating = true;
      setLineEditToSliderValue(i + 1, value);
      updating = false;
    });
  }
}

/**
 * @brief Validates text input and syncs it to sliders.
 *
 * Applies a QIntValidator (0-180) to the text fields. When editing finishes,
 * it updates the corresponding slider and ensures the value is strictly bounded.
 */
void RobotControlDialog::connectLineEditsToSliders()
{
  QLineEdit* lineEdits[] = {ui->lineEditMotor1, ui->lineEditMotor2, ui->lineEditMotor3, ui->lineEditMotor4, ui->lineEditMotor5, ui->lineEditMotor6};

  QIntValidator* validator = new QIntValidator(0, 180, this);
  for (int i = 0; i < 6; ++i) {
    lineEdits[i]->setValidator(validator);
  }
  QSlider* sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2, ui->horizontalSlider3,
                        ui->horizontalSlider4, ui->horizontalSlider5, ui->horizontalSlider6};
  for (int i = 0; i < 6; ++i) {
    connect(lineEdits[i], &QLineEdit::editingFinished, this, [lineEdit = lineEdits[i], slider = sliders[i]]() {
      int value = lineEdit->text().toInt();
      if (value < 0 || value > 180) {
        value = qBound(0, value, 180);
      }
      slider->setValue(value);
      lineEdit->setText(QString::number(value));
    });
  }
}

/**
 * @brief Updates the UI and Data Model when a slider moves.
 *
 * This function performs three tasks:
 * 1. Updates the `desiredAngle` in the `RobotSettings` struct.
 * 2. Updates the text in the corresponding QLineEdit.
 * 3. If in "Single" mode (!m_isAll), emits `motorAngleChanged` to move the robot immediately.
 *
 * @param[in] motorIndex The 1-based index of the motor.
 * @param[in] value The new angle value (0-180).
 */
void RobotControlDialog::setLineEditToSliderValue(int motorIndex, int value)
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  if (m_robotSettings) {
    m_robotSettings->motors[motorIndex - 1].desiredAngle = value;
  }

  switch (motorIndex) {
    case 1:
      ui->lineEditMotor1->setText(QString::number(value));
      break;
    case 2:
      ui->lineEditMotor2->setText(QString::number(value));
      break;
    case 3:
      ui->lineEditMotor3->setText(QString::number(value));
      break;
    case 4:
      ui->lineEditMotor4->setText(QString::number(value));
      break;
    case 5:
      ui->lineEditMotor5->setText(QString::number(value));
      break;
    case 6:
      ui->lineEditMotor6->setText(QString::number(value));
      break;
    default:
      emit errorOccurred("Invalid motor index");
      return;
  }

  if (!m_isAll) {
    int  motorValueToSend = value;
    emit motorAngleChanged(motorIndex, motorValueToSend);
  }
}

/**
 * @brief Resets the robot to its calibrated "Home" position.
 *
 * Loads the `defaultAngle` for all motors into the sliders and emits
 * `allMotorsReset` to move the physical robot.
 */
void RobotControlDialog::on_pushButtonReset_clicked()
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }
  setupOffsets();
  emit allMotorsReset();
}

/**
 * @brief Batch transmission of angles (Setup button).
 *
 * This is typically enabled in "All" mode. It reads all 6 slider values
 * and emits `motorAngleChanged` for every motor sequentially.
 */
void RobotControlDialog::on_pushButtonSetup_clicked()
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  int vals[6] = {ui->horizontalSlider1->value(), ui->horizontalSlider2->value(), ui->horizontalSlider3->value(),
                 ui->horizontalSlider4->value(), ui->horizontalSlider5->value(), ui->horizontalSlider6->value()};

  for (int i = 0; i < 6; ++i) {
    int  valToSend = vals[i];
    emit motorAngleChanged(i + 1, valToSend);
  }
}

/**
 * @brief Toggles between "Single" (Immediate) and "All" (Batch) control modes.
 *
 * - **Single**: Moving a slider moves the robot immediately.
 * - **All**: Moving sliders only updates the UI; the robot moves only when "Setup" is clicked.
 */
void RobotControlDialog::on_pushButtonAllSingle_clicked()
{
  m_isAll = !m_isAll;
  if (m_isAll) {
    ui->pushButtonAllSingle->setText("All");
    ui->pushButtonSetup->setEnabled(true);
  }
  else {
    ui->pushButtonAllSingle->setText("Single");
    ui->pushButtonSetup->setEnabled(false);
  }
}

/**
 * @brief Saves the current slider positions as the new "Home" offsets.
 *
 * Updates the `defaultAngle` in the settings model and notifies the system via
 * `motorOffsetChanged`. This effectively recalibrates the robot's zero position.
 */
void RobotControlDialog::on_pushButtonSetOffsets_clicked()
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  int vals[6] = {ui->horizontalSlider1->value(), ui->horizontalSlider2->value(), ui->horizontalSlider3->value(),
                 ui->horizontalSlider4->value(), ui->horizontalSlider5->value(), ui->horizontalSlider6->value()};

  for (int i = 0; i < 6; ++i) {
    m_robotSettings->motors[i].defaultAngle = vals[i];
    int  newOffset                          = vals[i];
    emit motorOffsetChanged(i + 1, newOffset);
  }
}

/**
 * @brief Loads the default angles from the settings into the UI sliders.
 * Effectively resets the visual controls to the "Home" position.
 */
void RobotControlDialog::setupOffsets()
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }
  ui->horizontalSlider1->setValue(m_robotSettings->motors[0].defaultAngle);
  ui->horizontalSlider2->setValue(m_robotSettings->motors[1].defaultAngle);
  ui->horizontalSlider3->setValue(m_robotSettings->motors[2].defaultAngle);
  ui->horizontalSlider4->setValue(m_robotSettings->motors[3].defaultAngle);
  ui->horizontalSlider5->setValue(m_robotSettings->motors[4].defaultAngle);
  ui->horizontalSlider6->setValue(m_robotSettings->motors[5].defaultAngle);
}

/**
 * @brief Saves the current slider positions as the "Place" target.
 *
 * Defines the specific pose the robot should assume when dropping an object.
 */
void RobotControlDialog::on_pushButtonSetPlace_clicked()
{
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  int vals[6] = {ui->horizontalSlider1->value(), ui->horizontalSlider2->value(), ui->horizontalSlider3->value(),
                 ui->horizontalSlider4->value(), ui->horizontalSlider5->value(), ui->horizontalSlider6->value()};
  for (int i = 0; i < 6; ++i) {
    int  position = vals[i];
    emit placePositionChanged(i + 1, position);
  }
}
