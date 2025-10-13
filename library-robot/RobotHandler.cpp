#include "RobotHandler.h"
#include <QDebug>
#include <QSettings>

RobotHandler &RobotHandler::instance() {
  static RobotHandler instance;
  return instance;
}

RobotHandler::RobotHandler(QObject *parent) : QObject(parent) {}

RobotHandler::~RobotHandler() {}
