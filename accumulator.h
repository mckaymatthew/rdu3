#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <QObject>
#include <QTimerEvent>

class Accumulator : public QObject
{
    Q_OBJECT
public:
    explicit Accumulator(QObject *parent = nullptr);
public slots:
    void input(int items);
    void setMax(int max);
signals:
    void output(int items);
protected:
    void timerEvent(QTimerEvent *event) override;
private:
    int accumulated = 0;
    int lastValue = 0;
    int maxValue = 0;
};

#endif // ACCUMULATOR_H
