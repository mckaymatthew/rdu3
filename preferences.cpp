#include "preferences.h"
#include "ui_preferences.h"
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHostAddress>

Preferences::Preferences(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Preferences),
    m_settings("KE0PSL", "RDU3")
{
    ui->setupUi(this);
    int maxVal = this->m_settings.value("maxVolume",(250/4)).toInt();
    auto deviceIp = this->m_settings.value("deviceIp","0.0.0.0").toString();

    this->ui->maxVolume->setValue(maxVal);
    auto percent = (double)maxVal/(double)this->ui->maxVolume->maximum() * 100.0;
    this->ui->volLabel->setText(QString("Maximum Volume: %1%").arg(percent,3,'f',0));

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression matchExpression("^" + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange + "$");
    this->ui->deviceIp->setValidator(new QRegularExpressionValidator(matchExpression));


    this->ui->deviceIp->setText(deviceIp);

    auto a = QHostAddress(deviceIp);
    if(a == QHostAddress::AnyIPv4) {
        this->ui->lookupAutomatic->setChecked(true);
        this->ui->lookupManual->setChecked(false);
        this->ui->deviceIp->setEnabled(false);
    } else {
        this->ui->lookupAutomatic->setChecked(false);
        this->ui->lookupManual->setChecked(true);
        this->ui->deviceIp->setEnabled(true);
    }


}

Preferences::~Preferences()
{
    delete ui;
}

void Preferences::on_maxVolume_valueChanged(int value)
{
    auto percent = (double)value/(double)this->ui->maxVolume->maximum() * 100.0;
    this->ui->volLabel->setText(QString("Maximum Volume: %1%").arg(percent,3,'f',0));
    this->m_settings.setValue("maxVolume",value);
}


void Preferences::on_done_clicked()
{
    if(this->ui->lookupAutomatic->isChecked()) {
        this->m_settings.setValue("deviceIp","0.0.0.0");
    }
    if(this->ui->lookupManual->isChecked()) {
        this->m_settings.setValue("deviceIp",this->ui->deviceIp->text());
    }
    emit done(QDialog::Accepted);
}

void Preferences::on_lookupAutomatic_clicked(bool checked)
{
    this->ui->deviceIp->setEnabled(false);
}


void Preferences::on_lookupManual_clicked(bool checked)
{
    this->ui->deviceIp->setEnabled(true);
}


void Preferences::on_fontSlider_valueChanged(int value)
{

}

