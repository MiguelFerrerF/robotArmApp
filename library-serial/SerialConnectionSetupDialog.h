#ifndef SERIALCONNECTIONSETUPDIALOG_H
#define SERIALCONNECTIONSETUPDIALOG_H

#include <QDialog>

namespace Ui
{
class SerialConnectionSetupDialog;
}

class SerialConnectionSetupDialog : public QDialog {
  Q_OBJECT

public:
  explicit SerialConnectionSetupDialog(QWidget* parent = nullptr);
  ~SerialConnectionSetupDialog();

signals:
  void errorOccurred(const QString& error);

private slots:
  void on_pushButtonConnect_clicked();
  void refreshPorts();

private:
  Ui::SerialConnectionSetupDialog* ui;
};

#endif // SERIALCONNECTIONSETUPDIALOG_H
