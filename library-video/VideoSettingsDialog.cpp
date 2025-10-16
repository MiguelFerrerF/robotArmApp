#include "VideoSettingsDialog.h"
#include "ui_VideoSettingsDialog.h"

VideoSettingsDialog::VideoSettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::VideoSettingsDialog) {
  ui->setupUi(this);
  setWindowTitle("Video Settings");
}

VideoSettingsDialog::~VideoSettingsDialog() { delete ui; }
