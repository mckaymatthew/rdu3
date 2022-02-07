#include "rduwindow.h"
#include "ui_rduwindow.h"

RDUWindow::RDUWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::RDUWindow)
{
    ui->setupUi(this);
}

RDUWindow::~RDUWindow()
{
    delete ui;
}
