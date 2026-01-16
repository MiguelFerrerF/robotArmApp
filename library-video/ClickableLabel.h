#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QPoint>

/**
 * @brief A custom QLabel that emits a signal when clicked by the mouse.
 *
 * Standard QLabels do not respond to mouse input. This subclass overrides
 * the mouse event handling to provide interactive functionality, allowing
 * the label to act like a button or an image map.
 */
class ClickableLabel : public QLabel
{
  Q_OBJECT
public:
  explicit ClickableLabel(QWidget* parent = nullptr);

signals:
  /**
   * @brief Emitted when the user presses the mouse button over the label.
   * @param pos The coordinates of the mouse cursor relative to the widget's top-left corner.
   */
  void clickedAt(const QPoint& pos);

protected:
  /**
   * @brief Overrides the default mouse press event.
   *
   * Intercepts the event to check for clicks and emit the signal,
   * while still allowing the default QLabel processing to occur.
   */
  void mousePressEvent(QMouseEvent* event) override;
};

#endif // CLICKABLELABEL_H
