#ifndef VIDEOSETTINGSDIALOG_H
#define VIDEOSETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class VideoSettingsDialog; }

class VideoSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoSettingsDialog(QWidget *parent = nullptr);
    ~VideoSettingsDialog();

private:
    Ui::VideoSettingsDialog *ui;
};

#endif // VIDEOSETTINGSDIALOG_H
