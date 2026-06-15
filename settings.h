#pragma once

#include <QDialog>
#include <QColor>
#include "kdropedit.h"

class QSpinBox;
class QKeySequenceEdit;
class QPushButton;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings &initial, QWidget *parent = nullptr);

    AppSettings settings() const;

signals:
    void languageChanged(int newLanguage);

private:
    void updateColorButton(QPushButton *button, const QColor &color);

    QSpinBox *m_fontSpin = nullptr;
    QKeySequenceEdit *m_shortcutEdit = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QSpinBox *m_radiusSpin = nullptr;
    QSpinBox *m_opacitySpin = nullptr;
    QPushButton *m_backgroundButton = nullptr;
    QPushButton *m_textButton = nullptr;
    QPushButton *m_englishButton = nullptr;
    QPushButton *m_russianButton = nullptr;

    QColor m_backgroundColor;
    QColor m_textColor;
    int m_currentLanguage;
};