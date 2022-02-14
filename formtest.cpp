#include "formtest.h"
#include "ui_formtest.h"

Formtest::Formtest(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Formtest)
{
    ui->setupUi(this);
}

Formtest::~Formtest()
{
    delete ui;
}
