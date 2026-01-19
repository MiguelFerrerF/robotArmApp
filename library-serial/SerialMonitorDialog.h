/**
 * @file SerialMonitorDialog.h
 * @author Miguel Ferrer
 * @brief  A real-time debugging console for Serial Port communication.
 *
 * This dialog acts as a terminal window. It subscribes to the SerialPortHandler
 * signals to display incoming and outgoing data streams. It also provides
 * a manual input field to send raw ASCII commands to the connected device.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef SERIALMONITORDIALOG_H
#define SERIALMONITORDIALOG_H

#include "SerialPortHandler.h"
#include "ui_SerialMonitorDialog.h"
#include <QDialog>
#include <QTextEdit>

namespace Ui
{
class SerialMonitorDialog;
}

/**
 * @brief A real-time debugging console for Serial Port communication.
 *
 * This dialog acts as a terminal window. It subscribes to the SerialPortHandler
 * signals to display incoming and outgoing data streams. It also provides
 * a manual input field to send raw ASCII commands to the connected device.
 */
class SerialMonitorDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SerialMonitorDialog(QWidget* parent = nullptr);
  ~SerialMonitorDialog();

  /**
   * @brief Accessor for the main log display widget.
   * Useful if external classes need to export logs or adjust visibility.
   * @return Pointer to the QTextEdit used for the console output.
   */
  QTextEdit* getLogView() const
  {
    return ui->textEditSerial;
  }

signals:
  /**
   * @brief Emitted when a user action fails (e.g., sending without connection).
   * @param message The warning description.
   */
  void warningOccurred(const QString& message);

private slots:
  /**
   * @brief UI Slot: Handles the "Send" button click.
   * Reads text from the line edit and transmits it via the SerialPortHandler.
   */
  void on_sendSerialButton_clicked();

  /**
   * @brief Slot: Handles incoming data from the physical serial port.
   * @param data The raw bytes received.
   */
  void onDataReceived(const QByteArray& data);

  /**
   * @brief Slot: Handles confirmation of data sent to the physical serial port.
   * @param data The raw bytes that were just transmitted.
   */
  void onDataSent(const QByteArray& data);

  void onCloseEvent(QCloseEvent* event);

private:
  Ui::SerialMonitorDialog* ui;
  bool                     m_serialConnected = false;
};

#endif // SERIALMONITORDIALOG_H
