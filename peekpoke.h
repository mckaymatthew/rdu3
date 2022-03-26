#ifndef PEEKPOKE_H
#define PEEKPOKE_H

#include <QDialog>
#include "rducontroller.h"

namespace Ui {
class PeekPoke;
}

class PeekPoke : public QDialog
{
    Q_OBJECT

public:
    explicit PeekPoke(QWidget *parent = nullptr);
    ~PeekPoke();
signals:
    void peek(RDUController::Register r);
    void poke(RDUController::Register r, uint32_t data);
public slots:
    void newData(RDUController::Register r, uint32_t data);
private slots:
    void on_peek_clicked();

    void on_poke_clicked();

private:
    Ui::PeekPoke *ui;
};

#endif // PEEKPOKE_H
