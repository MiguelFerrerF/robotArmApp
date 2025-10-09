#include "RobotControlDialog.h"
#include "ui_RobotControlDialog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPixmap>
#include <QSlider>
#include <QMessageBox>

RobotControlDialog::RobotControlDialog(QWidget *parent,
                                       RobotConfig::RobotSettings *settings)
    : QDialog(parent), ui(new Ui::RobotControlDialog),
      m_robotSettings(settings) {
  ui->setupUi(this);
  this->setWindowTitle("Robot Control");

  // Inicializar los QLineEdit y sliders en 90
  ui->horizontalSlider1->setValue(90);
  ui->horizontalSlider2->setValue(90);
  ui->horizontalSlider3->setValue(90);
  ui->horizontalSlider4->setValue(90);
  ui->horizontalSlider5->setValue(90);
  ui->horizontalSlider6->setValue(90);

  ui->lineEditMotor1Slider->setText("90");
  ui->lineEditMotor2Slider->setText("90");
  ui->lineEditMotor3Slider->setText("90");
  ui->lineEditMotor4Slider->setText("90");
  ui->lineEditMotor5Slider->setText("90");
  ui->lineEditMotor6Slider->setText("90");

  // Imagen principal del robot (se mantiene el QLabel de la imagen)
  QPixmap pixmap(QCoreApplication::applicationDirPath() + "/images/robot.png");
  ui->labelRobotImage->setPixmap(pixmap.scaled(500, 500, Qt::KeepAspectRatio));
  ui->labelRobotImage->setAlignment(Qt::AlignCenter);
  ui->labelRobotImage->setScaledContents(true);

  // Comunicación cruzada entre QLineEdit y QSlider con validación
  connect(ui->lineEditMotor1Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider1->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor1Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor1Slider->setText(QString::number(ui->horizontalSlider1->value()));
      }
  });
  connect(ui->lineEditMotor2Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider2->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor2Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor2Slider->setText(QString::number(ui->horizontalSlider2->value()));
      }
  });
  connect(ui->lineEditMotor3Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider3->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor3Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor3Slider->setText(QString::number(ui->horizontalSlider3->value()));
      }
  });
  connect(ui->lineEditMotor4Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider4->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor4Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor4Slider->setText(QString::number(ui->horizontalSlider4->value()));
      }
  });
  connect(ui->lineEditMotor5Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider5->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor5Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor5Slider->setText(QString::number(ui->horizontalSlider5->value()));
      }
  });
  connect(ui->lineEditMotor6Slider, &QLineEdit::textChanged, this, [this](const QString &text) {
      bool ok;
      int value = text.toInt(&ok);
      if (ok && value >= 0 && value <= 180) {
          ui->horizontalSlider6->setValue(value);
      } else if (ok) {
          QMessageBox::warning(ui->lineEditMotor6Slider, "Valor fuera de rango",
              "Por favor, introduzca un valor comprendido entre 0 y 180 grados.");
          ui->lineEditMotor6Slider->setText(QString::number(ui->horizontalSlider6->value()));
      }
  });

  // Cuando el usuario mueve el slider, actualiza el QLineEdit
  connect(ui->horizontalSlider1, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor1Slider->setText(QString::number(value));
  });
  connect(ui->horizontalSlider2, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor2Slider->setText(QString::number(value));
  });
  connect(ui->horizontalSlider3, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor3Slider->setText(QString::number(value));
  });
  connect(ui->horizontalSlider4, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor4Slider->setText(QString::number(value));
  });
  connect(ui->horizontalSlider5, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor5Slider->setText(QString::number(value));
  });
  connect(ui->horizontalSlider6, &QSlider::valueChanged, this, [this](int value) {
      ui->lineEditMotor6Slider->setText(QString::number(value));
  });
}

RobotControlDialog::~RobotControlDialog() { delete ui; }

void RobotControlDialog::on_pushButtonReset_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  // Reset sliders a los ángulos por defecto
  ui->horizontalSlider1->setValue(m_robotSettings->motors[0].defaultAngle);
  ui->horizontalSlider2->setValue(m_robotSettings->motors[1].defaultAngle);
  ui->horizontalSlider3->setValue(m_robotSettings->motors[2].defaultAngle);
  ui->horizontalSlider4->setValue(m_robotSettings->motors[3].defaultAngle);
  ui->horizontalSlider5->setValue(m_robotSettings->motors[4].defaultAngle);
  ui->horizontalSlider6->setValue(m_robotSettings->motors[5].defaultAngle);

  emit allMotorsReset();
}

void RobotControlDialog::on_pushButtonSetup_clicked() {
  if (!m_robotSettings) {
    emit errorOccurred("Robot settings not initialized.");
    return;
  }

  emit motorAngleChanged(1, ui->horizontalSlider1->value());
  emit motorAngleChanged(2, ui->horizontalSlider2->value());
  emit motorAngleChanged(3, ui->horizontalSlider3->value());
  emit motorAngleChanged(4, ui->horizontalSlider4->value());
  emit motorAngleChanged(5, ui->horizontalSlider5->value());
  emit motorAngleChanged(6, ui->horizontalSlider6->value());
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
