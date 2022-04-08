#ifndef FIRMWARE_H
#define FIRMWARE_H

#include <QDialog>
#include <QTimerEvent>
#include <ftdi.h>

namespace Ui {
class Firmware;
}

class Firmware : public QDialog
{
    Q_OBJECT

public:
    explicit Firmware(QWidget *parent = nullptr);
    ~Firmware();
private slots:
    void on_reset_clicked();

    void on_log_clicked();

    void on_gateware_verify_clicked();
    void on_gateware_erase_clicked();
    void on_gateware_program_clicked();
protected:
    void timerEvent(QTimerEvent *event) override;
private:
    bool jtagOpen();

    void genereateFlashImage();

    int timerId = -1;
    Ui::Firmware *ui;
    void updateLog(QString l);
    struct ftdi_context *ftdi = nullptr;

    bool readback = false;
    bool erase = false;
    uint32_t eraseLast = 0x0;
    bool program = false;
    uint32_t programLast = 0x0;
    QByteArray dataReadback;
    QByteArray dataFlashImage;

};

#endif // FIRMWARE_H
