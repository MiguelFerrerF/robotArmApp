#include "ClickableLabel.h"

ClickableLabel::ClickableLabel(QWidget* parent) : QLabel(parent)
{
}

/**
 * @brief Handles mouse press events to detect user interaction.
 *
 * If the Left Mouse Button is pressed, this function emits the `clickedAt` signal
 * with the local coordinates of the click.
 *
 * @note It explicitly calls `QLabel::mousePressEvent(event)` at the end.
 * This ensures that standard QLabel behaviors (like text selection or focus handling)
 * are preserved and not blocked by this override.
 *
 * @param event The mouse event details provided by the Qt event loop.
 */
void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    emit clickedAt(event->pos());

  QLabel::mousePressEvent(event);
}
