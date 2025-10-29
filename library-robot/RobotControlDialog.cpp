#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QIntValidator>
#include <QPixmap>
#include <QSlider>

RobotControlDialog::RobotControlDialog(QWidget *parent,
                                       RobotConfig::RobotSettings *settings)
    : QDialog(parent), ui(new Ui::RobotControlDialog),
      m_robotSettings(settings) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Control");

  // Imagen principal del robot (se mantiene el QLabel de la imagen)
  QPixmap pixmap(QCoreApplication::applicationDirPath() + "/images/robot.png");
  ui->labelRobotImage->setAlignment(Qt::AlignCenter);
  ui->labelRobotImage->setScaledContents(true);
  ui->labelRobotImage->setPixmap(pixmap.scaled(500, 500, Qt::KeepAspectRatio));

  connectSlidersToLineEdits();
  connectLineEditsToSliders();
}

RobotControlDialog::~RobotControlDialog() { delete ui; }

void RobotControlDialog::connectSlidersToLineEdits() {
  QSlider *sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2,
                        ui->horizontalSlider3, ui->horizontalSlider4,
                        ui->horizontalSlider5, ui->horizontalSlider6};
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

void RobotControlDialog::connectLineEditsToSliders() {

  // Solo permitir números entre 0 y 180 en los QLineEdit
  QLineEdit *lineEdits[] = {ui->lineEditMotor1, ui->lineEditMotor2,
                            ui->lineEditMotor3, ui->lineEditMotor4,
                            ui->lineEditMotor5, ui->lineEditMotor6};

  QIntValidator *validator = new QIntValidator(0, 180, this);
  for (int i = 0; i < 6; ++i) {
    lineEdits[i]->setValidator(validator);
  }
  QSlider *sliders[] = {ui->horizontalSlider1, ui->horizontalSlider2,
                        ui->horizontalSlider3, ui->horizontalSlider4,
                        ui->horizontalSlider5, ui->horizontalSlider6};
  for (int i = 0; i < 6; ++i) {
    connect(lineEdits[i], &QLineEdit::editingFinished, this,
            [lineEdit = lineEdits[i], slider = sliders[i]]() {
              int value = lineEdit->text().toInt();
              if (value < 0 || value > 180) {
                value = qBound(0, value, 180);
              }
              slider->setValue(value);
              lineEdit->setText(QString::number(value));
            });
  }
}

void RobotControlDialog::setLineEditToSliderValue(int motorIndex, int value) {
    if (!m_robotSettings) {
        emit errorOccurred("Robot settings not initialized.");
        return;
    }

    // Invertir dirección de motores 2 y 5
    if (motorIndex == 2 || motorIndex == 5) {
        value = 180 - value;  // Invertimos el sentido
    }

    switch (motorIndex) {
    case 1:
        ui->lineEditMotor1->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(1, value);
        break;
    case 2:
        ui->lineEditMotor2->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(2, value);
        break;
    case 3:
        ui->lineEditMotor3->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(3, value);
        break;
    case 4:
        ui->lineEditMotor4->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(4, value);
        break;
    case 5:
        ui->lineEditMotor5->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(5, value);
        break;
    case 6:
        ui->lineEditMotor6->setText(QString::number(value));
        if (!m_isAll)
            emit motorAngleChanged(6, value);
        break;
    default:
        emit errorOccurred("Invalid motor index");
        break;
    }

    if (m_robotSettings) {
        m_robotSettings->motors[motorIndex - 1].desiredAngle = value;
    }
}

void RobotControlDialog::on_pushButtonReset_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  ui->horizontalSlider1->setValue(m_robotSettings->motors[0].defaultAngle-60);
  ui->horizontalSlider2->setValue(m_robotSettings->motors[1].defaultAngle-13);
  ui->horizontalSlider3->setValue(m_robotSettings->motors[2].defaultAngle);
  ui->horizontalSlider4->setValue(m_robotSettings->motors[3].defaultAngle+44);
  ui->horizontalSlider5->setValue(m_robotSettings->motors[4].defaultAngle+21);
  ui->horizontalSlider6->setValue(m_robotSettings->motors[5].defaultAngle-90);

  emit allMotorsReset();
}

void RobotControlDialog::on_pushButtonSetup_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  // Tomamos los valores de los sliders
  int val1 = ui->horizontalSlider1->value();
  int val2 = ui->horizontalSlider2->value();
  int val3 = ui->horizontalSlider3->value();
  int val4 = ui->horizontalSlider4->value();
  int val5 = ui->horizontalSlider5->value();
  int val6 = ui->horizontalSlider6->value();

  // Invertimos solo al enviar los motores 2 y 5
  emit motorAngleChanged(1, val1);
  emit motorAngleChanged(2, 180 - val2);
  emit motorAngleChanged(3, val3);
  emit motorAngleChanged(4, val4);
  emit motorAngleChanged(5, 180 - val5);
  emit motorAngleChanged(6, val6);
}

void RobotControlDialog::on_pushButtonAllSingle_clicked() {
  m_isAll = !m_isAll;
  if (m_isAll) {
    ui->pushButtonAllSingle->setText("All");
    ui->pushButtonSetup->setEnabled(true);
  } else {
    ui->pushButtonAllSingle->setText("Single");
    ui->pushButtonSetup->setEnabled(false);
  }
}
