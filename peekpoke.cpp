#include "peekpoke.h"
#include "ui_peekpoke.h"
#include "QMetaEnum"

PeekPoke::PeekPoke(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PeekPoke)
{
    ui->setupUi(this);
    QMetaEnum e = QMetaEnum::fromType<RDUController::Register>();
    for (int k = 0; k < e.keyCount(); k++) {
        auto name = e.key(k);
        auto value = e.value(k);
        this->ui->reg->insertItem(this->ui->reg->count(),name,value);
    }
}

PeekPoke::~PeekPoke()
{
    delete ui;
}

void PeekPoke::newData(RDUController::Register, uint32_t data) {
    this->ui->value->setText(QString("%1").arg(data,8,16,QLatin1Char('0')).toUpper());
}
void PeekPoke::on_peek_clicked()
{
    emit peek((RDUController::Register)this->ui->reg->currentData().toInt());
}


void PeekPoke::on_poke_clicked()
{
    bool ok = false;
    emit poke((RDUController::Register)this->ui->reg->currentData().toInt(),
              this->ui->value->text().toUInt(&ok,16));
}

