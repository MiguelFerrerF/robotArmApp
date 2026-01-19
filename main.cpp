/**
 * @file main.cpp
 * @author Miguel Ferrer
 * @brief  Entry point for the Robot Arm Controller Application.
 *
 *  Initializes the QApplication, sets application metadata,
 * and launches the MainWindow.
 *
 * @version 0.1
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
  QApplication a(argc, argv);

  a.setWindowIcon(QIcon(":/images/icon.ico"));
  a.setApplicationName("Robot Arm Controller");
  a.setOrganizationName("Upna");
  a.setApplicationVersion("1.0.0");

  MainWindow w;
  w.show();
  return a.exec();
}
