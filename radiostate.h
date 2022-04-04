#ifndef RADIOSTATE_H
#define RADIOSTATE_H

#include <QObject>
#include <qqml.h>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QJSValue>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QMultiMap>

class RadioState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum FrontPanelButton {
        FPB_NB = 0x0e04,
        FPB_Notch = 0x0e02,
        FPB_NR = 0x0e08,
        FPB_PAmpAtt = 0x0e01,
        FPB_Transmit = 0x0d01,
        FPB_Tune = 0x0d02,
        FPB_VoxBkIn = 0x0d04,
        FPB_Menu = 0x0d08,
        FPB_Function = 0x0d10,
        FPB_MScope = 0x0d20,
        FPB_Quick = 0x0d40,
        FPB_Exit = 0x0e10,
        FPB_Clear = 0x0f40,
        FPB_Multi = 0x1004,
        FPB_MDown = 0x0f08,
        FPB_AB = 0x0f01,
        FPB_Split = 0x0f80,
        FPB_MPad = 0x0e80,
        FPB_RIT = 0x0f10,
        FPB_VM = 0x0f02,
        FPB_Tuner = 0x0e20,
        FPB_DeltaTx = 0x0f20,
        FPB_MUp = 0x0f04,
        FPB_SpLock = 0x0e40,
        FPB_XFC = 0x0d80
    };
    Q_ENUM(FrontPanelButton);
    explicit RadioState(QObject *parent = nullptr);
    ~RadioState();
    Q_INVOKABLE void press(FrontPanelButton button);
    Q_INVOKABLE void touch(QPoint p);
    Q_INVOKABLE void schedule(int delay, QJSValue callback);
    Q_INVOKABLE QString readText(QRect p, bool greyscale = true, bool invert = false);
    Q_INVOKABLE QColor pixel(QPoint p);

signals:
    void buffDispose(QByteArray* d);
    void injectData(QByteArray b);
    void injectTouch(QPoint p);
    void injectTouchRelease();
    void injectMultiDial(int ticks);
public slots:
    void newBuff(QByteArray* f);
protected:
    void timerEvent(QTimerEvent *event) override;
private:
    QByteArray* m_buffLast = nullptr;

    tesseract::TessBaseAPI *api;
    uint32_t m_workQueueIdx;
    QMultiMap<uint32_t,QJSValue> m_workQueue;
signals:

};

#endif // RADIOSTATE_H
