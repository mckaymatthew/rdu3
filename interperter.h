#ifndef INTERPERTER_H
#define INTERPERTER_H

#include <QDialog>

namespace Ui {
class Interperter;
}

class Interperter : public QDialog
{
    Q_OBJECT

public:
    explicit Interperter(QWidget *parent = nullptr);
    ~Interperter();
signals:
    void buffDispose(QByteArray* d);
public slots:
    void newBuff(QByteArray* f);
private:
    Ui::Interperter *ui;
    bool isHomeScreen = false;

    struct {
        QString screenName;
        QColor color;
        QList<QPoint> points;
    } typedef screenMatch_t;
    QList<screenMatch_t> m_screenMappings;
};

#endif // INTERPERTER_H
