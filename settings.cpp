#include "settings.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QButtonGroup>

SettingsDialog::SettingsDialog(const AppSettings &initial, QWidget *parent)
    : QDialog(parent)
    , m_backgroundColor(initial.backgroundColor)
    , m_textColor(initial.textColor)
    , m_currentLanguage(initial.language)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(500);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    root->addLayout(form);

    m_fontSpin = new QSpinBox(this);
    m_fontSpin->setRange(6, 72);
    m_fontSpin->setValue(initial.fontPointSize);
    form->addRow(tr("Font size"), m_fontSpin);

    m_shortcutEdit = new QKeySequenceEdit(this);
    m_shortcutEdit->setKeySequence(initial.toggleShortcut);
    form->addRow(tr("Keyboard shortcut"), m_shortcutEdit);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(10, 100);
    m_widthSpin->setSuffix(QStringLiteral(" %"));
    m_widthSpin->setValue(initial.widthPercent);
    form->addRow(tr("Window width"), m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(10, 100);
    m_heightSpin->setSuffix(QStringLiteral(" %"));
    m_heightSpin->setValue(initial.heightPercent);
    form->addRow(tr("Window height"), m_heightSpin);

    m_radiusSpin = new QSpinBox(this);
    m_radiusSpin->setRange(0, 40);
    m_radiusSpin->setValue(initial.cornerRadius);
    form->addRow(tr("Corner radius"), m_radiusSpin);

    m_opacitySpin = new QSpinBox(this);
    m_opacitySpin->setRange(20, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setValue(initial.opacityPercent);
    form->addRow(tr("Opacity"), m_opacitySpin);

    auto *colorRow = new QWidget(this);
    auto *colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    m_backgroundButton = new QPushButton(this);
    m_textButton = new QPushButton(this);
    updateColorButton(m_backgroundButton, m_backgroundColor);
    updateColorButton(m_textButton, m_textColor);
    colorLayout->addWidget(m_backgroundButton);
    colorLayout->addWidget(m_textButton);
    form->addRow(tr("Colors"), colorRow);

    connect(m_backgroundButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_backgroundColor, this, tr("Select background color"));
        if (chosen.isValid()) {
            m_backgroundColor = chosen;
            updateColorButton(m_backgroundButton, m_backgroundColor);
        }
    });

    connect(m_textButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_textColor, this, tr("Select text color"));
        if (chosen.isValid()) {
            m_textColor = chosen;
            updateColorButton(m_textButton, m_textColor);
        }
    });

    auto *langRow = new QWidget(this);
    auto *langLayout = new QHBoxLayout(langRow);
    langLayout->setContentsMargins(0, 0, 0, 0);

    m_englishButton = new QPushButton(tr("English"), this);
    m_russianButton = new QPushButton(tr("Russian"), this);
    m_englishButton->setCheckable(true);
    m_russianButton->setCheckable(true);

    auto *langGroup = new QButtonGroup(this);
    langGroup->addButton(m_englishButton, 0);
    langGroup->addButton(m_russianButton, 1);
    langGroup->setExclusive(true);

    if (m_currentLanguage == 1)
        m_russianButton->setChecked(true);
    else
        m_englishButton->setChecked(true);

    connect(langGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        if (id != m_currentLanguage) {
            emit languageChanged(id);
        }
    });

    langLayout->addWidget(m_englishButton);
    langLayout->addWidget(m_russianButton);
    form->addRow(tr("Language"), langRow);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::updateColorButton(QPushButton *button, const QColor &color)
{
    button->setText(color.name());
    button->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "background-color: %1;"
            "color: %2;"
            "padding: 6px 10px;"
            "border-radius: 6px;"
            "border: 1px solid rgba(127,127,127,0.6);"
            "}")
            .arg(color.name(),
                 (color.lightness() < 128) ? QStringLiteral("#ffffff") : QStringLiteral("#000000")));
}

AppSettings SettingsDialog::settings() const
{
    AppSettings s;
    s.fontPointSize = m_fontSpin->value();
    s.toggleShortcut = m_shortcutEdit->keySequence();
    s.widthPercent = m_widthSpin->value();
    s.heightPercent = m_heightSpin->value();
    s.cornerRadius = m_radiusSpin->value();
    s.opacityPercent = m_opacitySpin->value();
    s.backgroundColor = m_backgroundColor;
    s.textColor = m_textColor;
    s.language = m_currentLanguage;
    return s;
}