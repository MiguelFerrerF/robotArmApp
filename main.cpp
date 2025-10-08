#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  a.setWindowIcon(QIcon(":/images/icon.ico"));
  a.setApplicationName("Robot Arm Controller");
  a.setOrganizationName("Upna");
  a.setApplicationVersion("1.0.0");

  MainWindow w;
  w.show();
  return a.exec();
}
