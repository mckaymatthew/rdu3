#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QPoint>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());
signals:
    void touch(QPoint l);
    void release();
protected:
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
private:
    bool active = false;
};

#endif // CLICKABLELABEL_H
