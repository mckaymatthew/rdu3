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
signals:
    void output(int items);
protected:
    void timerEvent(QTimerEvent *event) override;
private:
    int accumulated = 0;
};

#endif // ACCUMULATOR_H
