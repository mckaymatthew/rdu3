#ifndef RDUWINDOW_H
#define RDUWINDOW_H

#include <QMainWindow>

namespace Ui {
class RDUWindow;
}

class RDUWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RDUWindow(QWidget *parent = nullptr);
    ~RDUWindow();

private:
    Ui::RDUWindow *ui;
};

#endif // RDUWINDOW_H
