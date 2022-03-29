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

class RadioState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentScreen MEMBER m_currentScreen NOTIFY screenChanged)
    Q_PROPERTY(QString currentSecondScreen MEMBER m_currentSecondScreen NOTIFY secondScreenChanged)
    QML_ELEMENT
public:
    enum FrontPanelButton {
        Exit = 0x0e10,
        Multi = 0x1004,
    };
    Q_ENUM(FrontPanelButton);
    explicit RadioState(QObject *parent = nullptr);
    Q_INVOKABLE void onScreen(QString screen, QJSValue jsCallback, int timeout = 0, QJSValue onTimeout = QJSValue());
    Q_INVOKABLE void onScreen(QString screen, QString secondScreen, QJSValue jsCallback);
    Q_INVOKABLE void press(FrontPanelButton button);
    Q_INVOKABLE void touch(int x, int y);

signals:
    void screenChanged(const QString & screen);
    void secondScreenChanged(const QString & screen);
    void buffDispose(QByteArray* d);
    void injectData(QByteArray b);
    void injectTouch(QPoint p);
    void injectTouchRelease();
public slots:
    void newBuff(QByteArray* f);
private:
    QString m_currentScreen;
    QString m_currentSecondScreen;
    struct {
        QColor color;
        QList<QPoint> points;
    } typedef pattern_t;
    struct {
        QString screenName;
        QString secondScreenName;
        QList<pattern_t> patterns;
    } typedef screenMatch_t;
    struct {
        QString screenName;
        bool secondScreen;
        QString secondScreenName;
        QJSValue jsCallback;

        QTimer* timeout;
    } typedef callback_t;
    QList<screenMatch_t> m_screenMappings;
    QList<callback_t> m_callbacks;
signals:

};

#endif // RADIOSTATE_H
