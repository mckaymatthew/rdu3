#include "radiostate.h"
#include "RDUConstants.h"
#include <QImage>
#include <QTimer>
namespace {
    /*!
     * Convert QT QImage to PIX
     * input: QImage
     * result: PIX
     */
    PIX* qImage2PIX(const QImage& qImage) {
      PIX * pixs;

      QImage myImage = qImage.rgbSwapped();
      int width = myImage.width();
      int height = myImage.height();
      int depth = myImage.depth();
      int wpl = myImage.bytesPerLine() / 4;

      pixs = pixCreate(width, height, depth);
      pixSetWpl(pixs, wpl);
      pixSetColormap(pixs, NULL);
      l_uint32 *datas = pixs->data;

      for (int y = 0; y < height; y++) {
        l_uint32 *lines = datas + y * wpl;
        QByteArray a((const char*)myImage.scanLine(y), myImage.bytesPerLine());
        for (int j = 0; j < a.size(); j++) {
          *((l_uint8 *)lines + j) = a[j];
        }
      }

      const qreal toDPM = 1.0 / 0.0254;
      int resolutionX = myImage.dotsPerMeterX() / toDPM;
      int resolutionY = myImage.dotsPerMeterY() / toDPM;

      if (resolutionX < 300) resolutionX = 300;
      if (resolutionY < 300) resolutionY = 300;
      pixSetResolution(pixs, resolutionX, resolutionY);

      return pixEndianByteSwapNew(pixs);
    }
}
RadioState::RadioState(QObject *parent) : QObject(parent)
{
    qmlRegisterType<RadioState>("FrontPanelButtons", 1, 0, "FrontPanelButton");
    api = new tesseract::TessBaseAPI();
    if (api->Init(NULL, "eng")) {
        qWarning() << "Tesseract Init FAIL " << api->Version();
        delete api;
        api = nullptr;
    } else {
        qInfo() << "Tesseract Init OK " << api->Version();
    }
    this->startTimer(33);
}
RadioState::~RadioState() {
    if(api != nullptr) {
        api->End();
        delete api;
    }
}
void RadioState::timerEvent(QTimerEvent *) {
    m_workQueueIdx++;
    auto items = m_workQueue.values(m_workQueueIdx);
    for(auto & item: items) {
        item.call();
    }
    m_workQueue.remove(m_workQueueIdx);
}

void RadioState::schedule(int delay, QJSValue callback) {
    if(!callback.isCallable()) {
        qWarning() << "Provided callback is not callable, did not schedule";
        return;
    }
    m_workQueue.insert(m_workQueueIdx + 1 + delay,callback);
}

void RadioState::newBuff(QByteArray* f) {
    if(m_buffLast != nullptr) {
        emit buffDispose(m_buffLast);
        m_buffLast = nullptr;
    }
    m_buffLast = f;
}

void RadioState::press(FrontPanelButton button) {
    if(button == 0) {
        qWarning() << "Failed to press button, unrecongized button in call";
        return;
    }
    qInfo() << "Button pressed from QML: " << button;
    uint16_t lesserButton = (uint16_t)button;
    auto enableStr = QString("fe%1fd").arg(lesserButton,4,16,QLatin1Char('0'));
    auto disableStr = enableStr;
    disableStr.replace(4,2,"00");
    emit injectData(QByteArray::fromHex(enableStr.toLatin1()));

    QTimer::singleShot(99, [disableStr, this](){
        emit injectData(QByteArray::fromHex(disableStr.toLatin1()));
    });
}

void RadioState::touch(QPoint p) {
    emit injectTouch(p);
    QTimer::singleShot(99, [this](){
        emit this->injectTouchRelease();
    });
}

QString RadioState::readText(QRect p, bool greyscale, bool invert) {
    if(api == nullptr) {
        return "Error, Tesseract failed to initialize";
    }
    if(m_buffLast == nullptr) {
        return "Error no buffer";
    }
    auto img = QImage((const uchar*) m_buffLast->data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    if(greyscale) img = img.convertToFormat(QImage::Format_Grayscale8);
    if(invert) img.invertPixels(QImage::InvertRgba);

    auto pix = qImage2PIX(img);
    api->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    api->SetImage(pix);
    api->SetRectangle(p.x(), p.y(), p.width(), p.height());
    auto outUtf8 = api->GetUTF8Text();
    auto outText = QString(outUtf8).trimmed();
    qInfo() << "OCR Result: " << outText;
    delete outUtf8;
    pixDestroy(&pix);
    return outText;
}

QColor RadioState::pixel(QPoint p) {
    auto img = QImage((const uchar*) m_buffLast->data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    auto ret = img.pixelColor(p);
    return ret;
}
