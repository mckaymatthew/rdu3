#include "accumulator.h"

Accumulator::Accumulator(QObject *parent) : QObject(parent)
{
    constexpr int wheelyBuffer = 1000/30; //30 times a second send the updates
    startTimer(wheelyBuffer);
}

void Accumulator::timerEvent(QTimerEvent *event) {
    if(accumulated != 0) {
        emit output(accumulated);
        accumulated = 0;
    }
}

void Accumulator::input(int items) {
    auto diff = items - lastValue;
    auto half = (maxValue/2);
    if(diff < -half) {
        diff = diff + maxValue;
    }
    if(diff > half) {
        diff = diff - maxValue;
    }
    lastValue = items;

    accumulated = accumulated + diff;
}

void Accumulator::setMax(int max) {
    maxValue = max;
}
