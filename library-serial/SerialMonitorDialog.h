#ifndef SERIALMONITORDIALOG_H
#define SERIALMONITORDIALOG_H

#include "ui_SerialMonitorDialog.h"
#include <QDialog>
#include <QTextEdit>

namespace Ui
{
class SerialMonitorDialog;
}

class SerialMonitorDialog : public QDialog {
  Q_OBJECT

public:
  explicit SerialMonitorDialog(QWidget* parent = nullptr);
  ~SerialMonitorDialog();

  QTextEdit* getLogView() const
  {
    return ui->textEditSerial;
  }

signals:
  void warningOccurred(const QString& message);

private slots:
  void onDataReceived(const QByteArray& data);
  void onDataSent(const QByteArray& data);
  void onStopSerialButtonClicked(bool checked);
  void onCloseEvent(QCloseEvent* event);

private:
  enum ButtonState
  {
    Start,
    Stop
  }; // Estados del botón

  Ui::SerialMonitorDialog* ui;
  bool                     m_serialConnected = false;

  void updateButtonState(ButtonState state); // Método para actualizar el botón
  bool serialStarted = false;                // Estado del botón de inicio/detener
};

#endif // SERIALMONITORDIALOG_H
