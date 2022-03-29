#include "radiostate.h"
#include "RDUConstants.h"
#include <QImage>
#include <QTimer>

RadioState::RadioState(QObject *parent) : QObject(parent)
{
    m_screenMappings.append({
       .screenName = "Home",
       .patterns = {
        {.color = QColor(255,255,255),
         .points = {QPoint(452,6), QPoint(452,7), QPoint(452,14), QPoint(452,15) } }}
                 //Look for the : in the timer
   });
    m_screenMappings.append({
       .screenName = "Menu",
       .patterns = {
        {.color = QColor(255,255,255),
         .points = {QPoint(218,14), QPoint(222,14), QPoint(225,14), QPoint(229,14), QPoint(234,14), QPoint(243,14),
                   QPoint(250,14), QPoint(255,14), QPoint(262,14)}}}
                  //Look for the text MENU text at the top
   });

    {
        QColor verticalMenuLineColor(74,73,74);
        QList<QPoint> verticalMenuLine;
        for(int i = 0; i < 270; i = i + 2) {
            verticalMenuLine.append(QPoint(361,i));
        }
        QColor selectedLineColor(90,162,206);
        QList<QPoint> rfPowerSelected;
        QList<QPoint> micGainSelected;
        QList<QPoint> monitorGainSelected;

        for(int i = 420; i < 425; i++) {
            rfPowerSelected.append(QPoint(i,20));
            micGainSelected.append(QPoint(i,76));
            monitorGainSelected.append(QPoint(i,191));
        }
        m_screenMappings.append({
           .screenName = "Multi",
           .secondScreenName = "RF Power",
           .patterns = {
            {.color = verticalMenuLineColor,
             .points = verticalMenuLine},
            {.color = selectedLineColor,
             .points = rfPowerSelected}}
       });
        m_screenMappings.append({
           .screenName = "Multi",
           .secondScreenName = "Mic Gain",
           .patterns = {
            {.color = verticalMenuLineColor,
             .points = verticalMenuLine},
            {.color = selectedLineColor,
             .points = micGainSelected}}
       });
        m_screenMappings.append({
           .screenName = "Multi",
           .secondScreenName = "Monitor Gain",
           .patterns = {
            {.color = verticalMenuLineColor,
             .points = verticalMenuLine},
            {.color = selectedLineColor,
             .points = monitorGainSelected}}
       });
    }
    qmlRegisterType<RadioState>("FrontPanelButtons", 1, 0, "FrontPanelButton");
}


void RadioState::onScreen(QString screen, QJSValue jsCallback, int timeout, QJSValue onTimeout) {
    if(!jsCallback.isCallable() || (timeout != 0 && !onTimeout.isCallable())) {
        qWarning() << "Provided callback is not callable, not proceeding";
        return;
    }
    QTimer* timeoutTimer = nullptr;
    if(timeout != 0) {
        timeoutTimer = new QTimer(this); //TODO is this leaking timers?
        timeoutTimer->setSingleShot(true);
        connect(timeoutTimer, &QTimer::timeout, [onTimeout](){
            qInfo() << "Callback in timeout";
            onTimeout.call();
        });
        timeoutTimer->start(timeout);
    }
    m_callbacks.append({screen, false, "", jsCallback, timeoutTimer});
}
void RadioState::onScreen(QString screen, QString secondScreen, QJSValue jsCallback) {
    if(!jsCallback.isCallable()) {
        qWarning() << "Provided callback is not callable, not proceeding";
        return;
    }
    m_callbacks.append({screen, true, secondScreen, jsCallback, nullptr});
}
void RadioState::newBuff(QByteArray* f) {
    QString newScreen = "Unknown";
    QString newSecondScreen = "Unknown";
    auto img = QImage((const uchar*) f->data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    for(auto& mapping: m_screenMappings) {
        bool matches = true;
        for(auto& pattern: mapping.patterns) {
            auto matchC = pattern.color;
            for(auto p: pattern.points) {
                auto c = img.pixelColor(p);
                if(c != matchC) {
                    matches = false;
                    break;
                }
            }
            if(!matches) {
                break;
            }
        }
        if(matches) {
            newScreen = mapping.screenName;
            newSecondScreen = mapping.secondScreenName;
            break;
        }
    }
    bool emitChanged = m_currentScreen != newScreen;
    bool emitSecondChanged = m_currentSecondScreen != newSecondScreen;
    m_currentScreen = newScreen;
    m_currentSecondScreen = newSecondScreen;
    emit buffDispose(f);
    if(emitChanged) emit screenChanged(newScreen);
    if(emitSecondChanged) emit secondScreenChanged(newSecondScreen);

    QList<callback_t> copyList = m_callbacks; //Callbacks can mutate iterator, tricky business.
    m_callbacks.clear(); //Clear out the callbacks
    QMutableListIterator<callback_t> i(copyList);
    while (i.hasNext()) {
        auto cb = i.next();
        bool remove = false;
        if(cb.timeout != nullptr) {
            bool timedOut = !cb.timeout->isActive();
            if(timedOut) {
                remove = true;
            }
        }
        if(!remove &&
                (cb.screenName == newScreen
                 && (!cb.secondScreen
                     || cb.secondScreenName == newSecondScreen))) {
            qInfo() << "Success: Callback for for  " << newScreen << " " << newSecondScreen << " activated.";
            cb.jsCallback.call();
            remove = true;
        }
        if(remove) {
            if(cb.timeout != nullptr) {
                cb.timeout->stop();
            }
            i.remove();
        }
    }
    m_callbacks.append(copyList);
}

void RadioState::press(FrontPanelButton button) {
    qInfo() << "Button pressed from QML: " << button;
    uint16_t lesserButton = (uint16_t)button;
    auto enableStr = QString("fe%1fd").arg(lesserButton,4,16,QLatin1Char('0'));
    auto disableStr = enableStr;
    disableStr.replace(4,2,"00");
    emit injectData(QByteArray::fromHex(enableStr.toLatin1()));

    QTimer::singleShot(100, [disableStr, this](){
        emit injectData(QByteArray::fromHex(disableStr.toLatin1()));
    });
}

void RadioState::touch(int x, int y) {
    emit injectTouch({x,y});
    QTimer::singleShot(100, [this](){
        emit this->injectTouchRelease();
    });
}
