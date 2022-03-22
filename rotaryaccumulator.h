#ifndef ROTARYACCUMULATOR_H
#define ROTARYACCUMULATOR_H

#include <QObject>
#include <QTimerEvent>

class RotaryAccumulator : public QObject
{
    Q_OBJECT
public:
    explicit RotaryAccumulator(QObject *parent = nullptr);
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

#endif // ROTARYACCUMULATOR_H
