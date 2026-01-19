/**
 * @file SerialConnectionSetupDialog.h
 * @author Miguel Ferrer
 * @brief  Dialog window for configuring the serial port connection.
 *
 * This class provides a graphical interface for the user to:
 * 1. Scan and list available serial ports on the system.
 * 2. Select a specific port and Baud Rate.
 * 3. Initiate the connection via the SerialPortHandler.
 * It manages setting persistence, ensuring that the last used port and
 * baud rate are remembered between application sessions.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef SERIALCONNECTIONSETUPDIALOG_H
#define SERIALCONNECTIONSETUPDIALOG_H

#include <QDialog>

namespace Ui
{
class SerialConnectionSetupDialog;
}

/**
 * @brief Dialog window for configuring the serial port connection.
 *
 * This class provides a graphical interface for the user to:
 * 1. Scan and list available serial ports on the system.
 * 2. Select a specific port and Baud Rate.
 * 3. Initiate the connection via the SerialPortHandler.
 *
 * It manages setting persistence, ensuring that the last used port and
 * baud rate are remembered between application sessions.
 */
class SerialConnectionSetupDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SerialConnectionSetupDialog(QWidget* parent = nullptr);
  ~SerialConnectionSetupDialog();

signals:
  /**
   * @brief Emitted when a connection attempt fails or input validation fails.
   * @param error A descriptive error message.
   */
  void errorOccurred(const QString& error);

private slots:
  /**
   * @brief Handles the "Connect" button click event.
   * Validates input, attempts connection, and saves settings on success.
   */
  void on_pushButtonConnect_clicked();

  /**
   * @brief Scans the system for serial ports and populates the combo box.
   * Auto-selects a port if it matches specific criteria (e.g., contains "UART").
   */
  void refreshPorts();

private:
  Ui::SerialConnectionSetupDialog* ui;
};

#endif // SERIALCONNECTIONSETUPDIALOG_H
