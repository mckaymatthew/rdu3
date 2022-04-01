#ifndef RADIOSTATE_H
#define RADIOSTATE_H

#include <QObject>
#include <qqml.h>
#include <QList>
#include <QColor>
#include <QPoint>
#include <QJSValue>
#include <QTimer>
#include <QSharedPointer>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QMultiMap>

class RadioState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum FrontPanelButton {
        Exit = 0x0e10,
        Multi = 0x1004,
    };
    Q_ENUM(FrontPanelButton);
    explicit RadioState(QObject *parent = nullptr);
    Q_INVOKABLE void press(FrontPanelButton button);
    Q_INVOKABLE void touch(QPoint p);
    Q_INVOKABLE void schedule(int delay, QJSValue callback);
    Q_INVOKABLE QString readText(int x, int y, int w, int h, bool greyscale = true, bool invert = false);
    Q_INVOKABLE QColor pixel(QPoint p);


signals:
    void screenChanged(const QString & screen);
    void secondScreenChanged(const QString & screen);
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
