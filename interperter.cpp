#include "interperter.h"
#include "ui_interperter.h"
#include "RDUConstants.h"

Interperter::Interperter(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Interperter)
{
    ui->setupUi(this);

    m_screenMappings.append({
       .screenName = "Home",
       .color = QColor(255,255,255),
       .points = {QPoint(452,6), QPoint(452,7), QPoint(452,14), QPoint(452,15)}
   });
    m_screenMappings.append({
       .screenName = "Menu",
       .color = QColor(255,255,255),
       .points = {QPoint(218,14), QPoint(222,14), QPoint(225,14), QPoint(229,14), QPoint(234,14), QPoint(243,14),
                   QPoint(250,14), QPoint(255,14), QPoint(262,14)}
   });

    QList<QPoint> verticalMenuLine;
    for(int i = 0; i < 270; i = i + 2) {
        verticalMenuLine.append(QPoint(361,i));
    }
    m_screenMappings.append({
       .screenName = "Mutli Menu",
       .color = QColor(74,73,74),
       .points = verticalMenuLine
   });


}

Interperter::~Interperter()
{
    delete ui;
}

void Interperter::newBuff(QByteArray* f) {
    QString newScreen = "Unknown";
    auto i = QImage((const uchar*) f->data(), COLUMNS, LINES, COLUMNS * sizeof(uint16_t),QImage::Format_RGB16);
    for(auto mapping: m_screenMappings) {
        bool matches = true;
        for(auto p: mapping.points) {
            auto c = i.pixelColor(p);
            if(c != mapping.color) {
                matches = false;
            }
        }
        if(matches) {
            newScreen = mapping.screenName;
            break;
        }
    }
    this->ui->screen->setText(newScreen);
    emit buffDispose(f);
}
