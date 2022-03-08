#ifndef CSRMAP_H
#define CSRMAP_H

#include <QObject>

class CSRMap : public QObject
{
    Q_OBJECT
public:
    static CSRMap& get()
    {
        static CSRMap instance;
        return instance;
    }
    uint32_t CLK_GATE = 0xf0002800;
    uint32_t CPU_RESET = 0xf0000000;
    uint32_t FPS_DIVISOR = 0xf0002804;
    uint32_t MAIN_DAIL_OFFSET = 0xf0003000;
    bool updated;
private:
    explicit CSRMap(QObject *parent = nullptr);
    CSRMap(CSRMap const&)               = delete;
    void operator=(CSRMap const&)  = delete;
    void readCSRFile();

signals:

};

#endif // CSRMAP_H
