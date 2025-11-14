#include "ClickableLabel.h"

ClickableLabel::ClickableLabel(QWidget* parent) : QLabel(parent)
{
}

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit clickedAt(event->pos());

  QLabel::mousePressEvent(event);
}
