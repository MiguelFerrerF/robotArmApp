#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QPoint>

class ClickableLabel : public QLabel
{
  Q_OBJECT
public:
  explicit ClickableLabel(QWidget* parent = nullptr);

signals:
  void clickedAt(const QPoint& pos);

protected:
  void mousePressEvent(QMouseEvent* event) override;
};

#endif // CLICKABLELABEL_H
