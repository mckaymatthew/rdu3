#include "frontpanelrender.h"
#include "ui_frontpanelrender.h"

FrontPanelRender::FrontPanelRender(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::FrontPanelRender)
{
    ui->setupUi(this);
}

FrontPanelRender::~FrontPanelRender()
{
    delete ui;
}
