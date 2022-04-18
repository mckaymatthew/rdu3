#define NOMINMAX
#include "firmware.h"
#include "ui_firmware.h"
#include <QByteArray>
#include <QRegularExpression>
#include <QFile>
#include <QDebug>

namespace {
    bool sneaks;
    QString sneakyLog;
    QRegularExpression ansiEscape("\x1b\[[0-9;]*[a-zA-Z]");
    int vid = 0x403;
    int pid = 0x6010;
    int baudrate = 921600;
    ftdi_interface interface = INTERFACE_B; //Interface B is the UART


    const char *devstr = NULL;
    int ifnum = 0; //Interface A is JTAG
    int clkdiv = 1;

}
extern "C" {
    extern int mpsse_error_last;
    void qprintf ( const char * format, ... )
    {
        va_list args;
        va_start (args, format);
        QString formatted = QString::vasprintf(format, args).trimmed();
        sneaks = true;
        sneakyLog = sneakyLog + formatted;
        qInfo() << formatted;
        va_end (args);
    }
    #include "ecpprog.c"
}

Firmware::Firmware(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Firmware)
{
    ui->setupUi(this);
    timerId = startTimer(33);
}

Firmware::~Firmware()
{
    delete ui;
}

void Firmware::on_reset_clicked()
{


    updateLog("JTAG: Initialize interface");
//    this->ui->statusLog->setText(this->ui->statusLog->text() + "jtag init\n");
    jtag_init(ifnum, devstr, clkdiv);
    if(mpsse_error_last == 0) {
        read_idcode();
        read_status_register();

        updateLog("JTAG: Issue Device Refresh");
        ecp_jtag_cmd(LSC_REFRESH);
        updateLog("JTAG: Close interface");
        jtag_deinit();
    } else {
        updateLog("Failed to open device.");
        updateLog("USB device plugged in?");
    }
}

void Firmware::updateLog(QString l) {
    this->ui->statusLog->appendPlainText(l);
    qInfo() << l;

}
void Firmware::timerEvent(QTimerEvent *) {
    if(sneaks) {
        sneaks = false;
        updateLog(sneakyLog);
        sneakyLog = "";
    }
    if(ftdi != nullptr) {
        unsigned char buf[1024];
        auto f = ftdi_read_data(ftdi, buf, sizeof(buf));
        if(f > 0) {
            QString readResult = QString::fromLatin1((const char *)buf,f);
            readResult = readResult.replace(ansiEscape, "");
            updateLog(readResult);
        } else if (f < 0) {
            updateLog(QString("Error during read of serial port %1 (%2)").arg(f).arg(ftdi_get_error_string(ftdi)));
            updateLog(QString("Closing port"));
            ftdi_usb_close(ftdi);
            ftdi_free(ftdi);
            ftdi = nullptr;
            this->ui->log->setEnabled(true);
        }
    }
    bool closeJtag = false;
    if(readback) {
        auto amountToRead = std::min((int)1024, (int) (dataFlashImage.size() - dataReadback.size()));
        if(amountToRead == 0) {
            updateLog(QString("JTAG: Readback done (%1 of %2 bytes read)").arg(dataReadback.size()).arg(dataFlashImage.size()));
            updateLog(QString("JTAG: Flash Verify %1.").arg(dataReadback == dataFlashImage ? "PASS":"FAIL"));
            readback = false;
            closeJtag = true;
        } else {
            QByteArray buffer_flash;
            buffer_flash.fill('\0',amountToRead);
//            updateLog(QString("JTAG: Read %1 bytes").arg(amountToRead));
            flash_continue_read((uint8_t*)buffer_flash.data(), (int)amountToRead);
            dataReadback.append(buffer_flash);
        }
    }
    if(erase) {
        if(eraseLast == dataFlashImage.size()) {
            updateLog(QString("JTAG: Erase Done."));
            erase = false;
            closeJtag = true;
        } else {
            eraseLast = eraseLast + (64 << 10);
            flash_write_enable();
            flash_64kB_sector_erase(eraseLast);
            flash_wait();
            //TODO: this is a "blocking" operation
            //Since it can block the GUI can stall
            //However, the wait has a timeout of 2000uS, which is probably an OK time for the GUI to block
            //Not sure why this has a timeout or why the behavior on timeout is to just continue OK
            //Also not sure why sometimes the success/done bit is set and we don't timeout
        }
    }
    if(program) {
        if(programLast >= dataFlashImage.size()) {
            updateLog(QString("JTAG: JTAG Done."));
            program = false;
            closeJtag = true;
        } else {
            programLast = programLast + 256;
            QByteArray payload = dataFlashImage.mid(programLast,256);
            if(programLast % (64 << 10) == 0) {
                updateLog(QString("JTAG: Program %1\% %2/%3.")
                          .arg((int)((double)programLast/(double)dataFlashImage.size()*100.0))
                          .arg(programLast, 8, 16)
                          .arg(dataFlashImage.size(), 8, 16));
            }

            flash_write_enable();
            flash_prog(programLast, (uint8_t*)payload.data(), payload.size());
            flash_wait();
            //TODO: this is a "blocking" operation
            //Since it can block the GUI can stall
            //However, the wait has a timeout of 2000uS, which is probably an OK time for the GUI to block
            //Not sure why this has a timeout or why the behavior on timeout is to just continue OK
            //Also not sure why sometimes the success/done bit is set and we don't timeout
        }

    }
    if(closeJtag) {
        updateLog("JTAG: Issue Device Refresh");
        ecp_jtag_cmd(LSC_REFRESH);
        updateLog("JTAG: Close interface");
        jtag_deinit();
        killTimer(timerId);
        timerId = startTimer(33);
        this->ui->gateware_erase->setEnabled(true);
        this->ui->gateware_program->setEnabled(true);
        this->ui->gateware_verify->setEnabled(true);
        this->ui->reset->setEnabled(true);
    }
}

void Firmware::on_log_clicked()
{
    if ((ftdi = ftdi_new()) == 0)
    {
        updateLog("ftdi_new failed");
        ftdi = nullptr;
        return;
    }
    // Select interface
    ftdi_set_interface(ftdi, interface);

    updateLog("Open Console Device");
    // Open device
    auto f = ftdi_usb_open(ftdi, vid, pid);
    if (f < 0)
    {
        updateLog(QString("Failed to open FTDI device: %1 (%2)").arg(f).arg(ftdi_get_error_string(ftdi)));
        updateLog("USB device plugged in?");
        ftdi_free(ftdi);
        ftdi = nullptr;
        return;
    }
    updateLog("Open OK, streaming console");
    ftdi_set_baudrate(ftdi, baudrate);
    ftdi_set_line_property(ftdi, BITS_8, STOP_BIT_1, NONE);
    this->ui->log->setEnabled(false);

}


bool Firmware::jtagOpen() {

    dataReadback.clear();
    genereateFlashImage();

    updateLog("JTAG: Initialize interface");
//    this->ui->statusLog->setText(this->ui->statusLog->text() + "jtag init\n");
    jtag_init(ifnum, devstr, clkdiv);
    if(mpsse_error_last == 0) {
        this->ui->gateware_erase->setEnabled(false);
        this->ui->gateware_program->setEnabled(false);
        this->ui->gateware_verify->setEnabled(false);
        this->ui->reset->setEnabled(false);

        updateLog("JTAG: ECP5 to reset");
        /* Reset ECP5 to release SPI interface */
        ecp_jtag_cmd8(ISC_ENABLE, 0);
        ecp_jtag_cmd8(ISC_ERASE, 0);
        ecp_jtag_cmd8(ISC_DISABLE, 0);

        /* Put device into SPI bypass mode */
        enter_spi_background_mode();
        flash_reset();
        killTimer(timerId);
        timerId = startTimer(0);
        return true;
    }
    return false;
}
void Firmware::on_gateware_verify_clicked()
{
    if(jtagOpen()) {
        flash_start_read(0x0);
        readback = true;
        updateLog(QString("Starting flash read of size 0x%1").arg(dataFlashImage.size(),8,16,QLatin1Char('0')));
    }
}

void Firmware::on_gateware_erase_clicked()
{
    if(jtagOpen()) {
        eraseLast = 0x0;
        erase = true;
        flash_write_enable();
        flash_64kB_sector_erase(eraseLast);
        flash_wait();

        updateLog(QString("Starting flash erase of size 0x%1").arg(dataFlashImage.size(),8,16,QLatin1Char('0')));
    }
}
void Firmware::on_gateware_program_clicked() {
    if(jtagOpen()) {
        programLast = 0x0;
        program = true;
        flash_write_enable();

        QByteArray payload = dataFlashImage.mid(programLast,256);
        flash_prog(programLast, (uint8_t*)payload.data(), payload.size());
        flash_wait();
        updateLog(QString("Starting flash program of size 0x%1").arg(dataFlashImage.size(),8,16,QLatin1Char('0')));
    }
}

void Firmware::genereateFlashImage() {
    dataFlashImage.clear();
    QFile bitstream("/Users/mckaym/IC7300/litex/litex-boards/litex_boards/targets/build/colorlight_i5_msm/gateware/colorlight_i5_msm.bit");
    QFile softwareFbi("/Users/mckaym/Dropbox/IC7300/rdu_app/build/zephyr/zephyr.bin.fbi");
    bitstream.open(QFile::ReadOnly);
    softwareFbi.open(QFile::ReadOnly);
    auto gateware = bitstream.readAll();
    auto software = softwareFbi.readAll();

    //64k byte erase blocks (0x10,000)
    int block_size = 64 << 10;
    int block_mask = block_size - 1;
    int gatewareEndIdx = (gateware.size() + block_mask) & ~block_mask;
    int blocksPaddingMid = gatewareEndIdx - gateware.size();
//        updateLog(QString("Bytes: Gateware %1, Alignment %2, Software %3 bytes")
//                  .arg(gateware.size()).arg(blocksPadding).arg(software.size()));

    dataFlashImage.append(gateware);
    //Pad out to next erase block
    dataFlashImage.append(QByteArray(blocksPaddingMid,0xFF));
    dataFlashImage.append(software);
    int flashImageEndIdx = (dataFlashImage.size() + block_mask) & ~block_mask;
    int blocksPaddingEnd = flashImageEndIdx - dataFlashImage.size();
    dataFlashImage.append(QByteArray(blocksPaddingEnd,0xFF));

}


//void Firmware::on_gateware_dump_clicked()
//{
//    auto dataReadbackStr = dataReadback.toHex();
//    auto dataFlashImageStr = dataFlashImage.toHex();
//    auto step = 256*2;
//    for (int i = step; i <= dataReadbackStr.size(); i+=step+1)
//        dataReadbackStr.insert(i, '\n');
//    for (int i = step; i <= dataFlashImageStr.size(); i+=step+1)
//        dataFlashImageStr.insert(i, '\n');


//    QFile r("/Users/mckaym/dataReadback.bin");
//    QFile f("/Users/mckaym/dataFlashImage.bin");
//    f.open(QIODevice::WriteOnly);
//    r.open(QIODevice::WriteOnly);
//    f.write(dataFlashImageStr);
//    r.write(dataReadbackStr);
//    f.close();
//    r.close();
//}

