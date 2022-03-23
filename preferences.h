#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QDialog>
#include <QSettings>
#include <QWheelEvent>

namespace Ui {
class Preferences;
}

class Preferences : public QDialog
{
    Q_OBJECT

public:
    explicit Preferences(QWidget *parent = nullptr);
    ~Preferences();

private slots:
    void on_maxVolume_valueChanged(int value);
    void on_done_clicked();
    void on_lookupAutomatic_clicked(bool checked);

    void on_lookupManual_clicked(bool checked);

    void on_fontSlider_valueChanged(int value);

private:
    Ui::Preferences *ui;

    QSettings m_settings;
};

#endif // PREFERENCES_H
