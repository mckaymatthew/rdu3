#ifndef FRONTPANELRENDER_H
#define FRONTPANELRENDER_H

#include <QDockWidget>

namespace Ui {
class FrontPanelRender;
}

class FrontPanelRender : public QDockWidget
{
    Q_OBJECT

public:
    explicit FrontPanelRender(QWidget *parent = nullptr);
    ~FrontPanelRender();

private:
    Ui::FrontPanelRender *ui;
};

#endif // FRONTPANELRENDER_H
