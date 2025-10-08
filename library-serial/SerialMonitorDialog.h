#ifndef SERIALMONITORDIALOG_H
#define SERIALMONITORDIALOG_H

#include "ui_SerialMonitorDialog.h"
#include <QDialog>
#include <QTextEdit>

namespace Ui {
class SerialMonitorDialog;
}

class SerialMonitorDialog : public QDialog {
  Q_OBJECT

public:
  explicit SerialMonitorDialog(QWidget *parent = nullptr);
  ~SerialMonitorDialog();

  QTextEdit *getLogView() const { return ui->textEditSerial; }

signals:
  void warningOccurred(const QString &message);

private slots:
  void on_sendSerialButton_clicked();

  void onDataReceived(const QByteArray &data);
  void onDataSent(const QByteArray &data);
  void onCloseEvent(QCloseEvent *event);

private:
  Ui::SerialMonitorDialog *ui;
  bool m_serialConnected = false;
};

#endif // SERIALMONITORDIALOG_H
