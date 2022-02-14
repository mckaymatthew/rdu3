#ifndef FORMTEST_H
#define FORMTEST_H

#include <QWidget>

namespace Ui {
class Formtest;
}

class Formtest : public QWidget
{
    Q_OBJECT

public:
    explicit Formtest(QWidget *parent = nullptr);
    ~Formtest();

private:
    Ui::Formtest *ui;
};

#endif // FORMTEST_H
