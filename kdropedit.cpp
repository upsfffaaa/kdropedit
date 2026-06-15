#include "kdropedit.h"

#include <KGlobalAccel>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr auto kAppName = "kdropedit";

class DragHandleWidget final : public QWidget
{
public:
    explicit DragHandleWidget(QWidget *windowToMove, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_window(windowToMove)
    {
        setFixedSize(48, 28);
        setCursor(Qt::SizeAllCursor);
        setToolTip(QStringLiteral("Переместить окно"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(
            QStringLiteral(
                "QWidget {"
                "background-color: rgba(255,255,255,0.04);"
                "border: 1px solid rgba(127,127,127,0.6);"
                "border-radius: 6px;"
                "}"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(180, 180, 180), 1.4));

        const int cx = width() / 2;
        const int cy = height() / 2;

        for (int i = -2; i <= 2; ++i) {
            p.drawLine(QPointF(cx - 7, cy + i * 3), QPointF(cx + 7, cy + i * 3));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_window) {
            if (QWindow *win = m_window->windowHandle()) {
                win->startSystemMove();
                event->accept();
                return;
            }
            m_dragging = true;
            m_pressGlobal = event->globalPosition().toPoint();
            m_startPos = m_window->pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging && (event->buttons() & Qt::LeftButton) && m_window) {
            const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
            m_window->move(m_startPos + delta);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    QWidget *m_window = nullptr;
    bool m_dragging = false;
    QPoint m_pressGlobal;
    QPoint m_startPos;
};

void setColorButtonStyle(QPushButton *button, const QColor &color, const QString &prefix)
{
    const QString textColor = (color.lightness() < 128) ? QStringLiteral("#ffffff")
                                                        : QStringLiteral("#000000");
    button->setText(prefix + QStringLiteral(": ") + color.name());
    button->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "background-color: %1;"
            "color: %2;"
            "padding: 6px 10px;"
            "border-radius: 6px;"
            "border: 1px solid rgba(127,127,127,0.6);"
            "}")
            .arg(color.name(), textColor));
}

class SettingsDialog final : public QDialog
{
public:
    explicit SettingsDialog(const AppSettings &initial, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_backgroundColor(initial.backgroundColor)
        , m_textColor(initial.textColor)
    {
        setWindowTitle(QStringLiteral("Настройки"));
        setMinimumWidth(500);

        auto *root = new QVBoxLayout(this);
        auto *form = new QFormLayout();
        root->addLayout(form);

        m_fontSpin = new QSpinBox(this);
        m_fontSpin->setRange(6, 72);
        m_fontSpin->setValue(initial.fontPointSize);
        form->addRow(QStringLiteral("Размер шрифта"), m_fontSpin);

        m_shortcutEdit = new QKeySequenceEdit(this);
        m_shortcutEdit->setKeySequence(initial.toggleShortcut);
        form->addRow(QStringLiteral("Комбинация клавиш"), m_shortcutEdit);

        m_widthSpin = new QSpinBox(this);
        m_widthSpin->setRange(10, 100);
        m_widthSpin->setSuffix(QStringLiteral(" %"));
        m_widthSpin->setValue(initial.widthPercent);
        form->addRow(QStringLiteral("Ширина окна"), m_widthSpin);

        m_heightSpin = new QSpinBox(this);
        m_heightSpin->setRange(10, 100);
        m_heightSpin->setSuffix(QStringLiteral(" %"));
        m_heightSpin->setValue(initial.heightPercent);
        form->addRow(QStringLiteral("Высота окна"), m_heightSpin);

        m_radiusSpin = new QSpinBox(this);
        m_radiusSpin->setRange(0, 40);
        m_radiusSpin->setValue(initial.cornerRadius);
        form->addRow(QStringLiteral("Скругление окна"), m_radiusSpin);

        m_opacitySpin = new QSpinBox(this);
        m_opacitySpin->setRange(20, 100);
        m_opacitySpin->setSuffix(QStringLiteral(" %"));
        m_opacitySpin->setValue(initial.opacityPercent);
        form->addRow(QStringLiteral("Прозрачность"), m_opacitySpin);

        auto *colorRow = new QWidget(this);
        auto *colorLayout = new QHBoxLayout(colorRow);
        colorLayout->setContentsMargins(0, 0, 0, 0);

        m_backgroundButton = new QPushButton(this);
        m_textButton = new QPushButton(this);

        setColorButtonStyle(m_backgroundButton, m_backgroundColor, QStringLiteral(""));
        setColorButtonStyle(m_textButton, m_textColor, QStringLiteral(""));

        colorLayout->addWidget(m_backgroundButton);
        colorLayout->addWidget(m_textButton);
        form->addRow(QStringLiteral("Цвета"), colorRow);

        connect(m_backgroundButton, &QPushButton::clicked, this, [this]() {
            const QColor chosen = QColorDialog::getColor(m_backgroundColor, this, QStringLiteral("Выберите цвет фона"));
            if (chosen.isValid()) {
                m_backgroundColor = chosen;
                setColorButtonStyle(m_backgroundButton, m_backgroundColor, QStringLiteral("Цвет фона"));
            }
        });

        connect(m_textButton, &QPushButton::clicked, this, [this]() {
            const QColor chosen = QColorDialog::getColor(m_textColor, this, QStringLiteral("Выберите цвет текста"));
            if (chosen.isValid()) {
                m_textColor = chosen;
                setColorButtonStyle(m_textButton, m_textColor, QStringLiteral("Цвет текста"));
            }
        });

        auto *buttons = new QDialogButtonBox(this);
        buttons->addButton(QDialogButtonBox::Ok);
        buttons->addButton(QDialogButtonBox::Cancel);
        root->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    AppSettings settings() const
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
        return s;
    }

private:
    QSpinBox *m_fontSpin = nullptr;
    QKeySequenceEdit *m_shortcutEdit = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QSpinBox *m_radiusSpin = nullptr;
    QSpinBox *m_opacitySpin = nullptr;
    QPushButton *m_backgroundButton = nullptr;
    QPushButton *m_textButton = nullptr;

    QColor m_backgroundColor;
    QColor m_textColor;
};

static QString currentEditorFilePath(QPlainTextEdit *editor)
{
    return editor ? editor->property("filePath").toString() : QString();
}
} // namespace

DropdownEditorWindow::DropdownEditorWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(loadSettings())
{
    if (qApp) {
        qApp->setQuitOnLastWindowClosed(false);
    }

    setWindowTitle(QStringLiteral("Текстовый редактор"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_rootWidget = new QWidget(this);
    m_rootWidget->setObjectName(QStringLiteral("rootWidget"));
    m_rootWidget->setAttribute(Qt::WA_StyledBackground, false);
    m_rootWidget->setAutoFillBackground(false);

    auto *rootLayout = new QVBoxLayout(m_rootWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_headerWidget = new QWidget(m_rootWidget);
    m_headerWidget->setAttribute(Qt::WA_StyledBackground, false);
    m_headerWidget->setAutoFillBackground(false);

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 6, 8, 4);
    headerLayout->setSpacing(6);

    m_addTabButton = new QToolButton(m_headerWidget);
    m_addTabButton->setText(QStringLiteral("＋"));
    m_addTabButton->setAutoRaise(true);
    m_addTabButton->setCursor(Qt::PointingHandCursor);
    m_addTabButton->setToolTip(QStringLiteral("Новая вкладка"));
    m_addTabButton->setFixedSize(28, 28);

    m_tabBar = new QTabBar(m_headerWidget);
    m_tabBar->setMovable(true);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_dragHandle = new DragHandleWidget(this, m_headerWidget);

    m_settingsButton = new QToolButton(m_headerWidget);
    m_settingsButton->setText(QStringLiteral("⚙"));
    m_settingsButton->setAutoRaise(true);
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip(QStringLiteral("Настройки"));
    m_settingsButton->setFixedSize(28, 28);

    headerLayout->addWidget(m_addTabButton, 0, Qt::AlignVCenter);
    headerLayout->addWidget(m_tabBar, 1);
    headerLayout->addWidget(m_dragHandle, 0, Qt::AlignVCenter);
    headerLayout->addWidget(m_settingsButton, 0, Qt::AlignVCenter);

    auto *headerLine = new QFrame(m_rootWidget);
    headerLine->setFrameShape(QFrame::HLine);
    headerLine->setFrameShadow(QFrame::Sunken);
    headerLine->setStyleSheet(QStringLiteral("background: rgba(127,127,127,0.4);"));

    m_stack = new QStackedWidget(m_rootWidget);
    m_stack->setAttribute(Qt::WA_StyledBackground, false);
    m_stack->setAutoFillBackground(false);
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));

    rootLayout->addWidget(m_headerWidget);
    rootLayout->addWidget(headerLine);
    rootLayout->addWidget(m_stack, 1);

    setCentralWidget(m_rootWidget);

    connect(m_addTabButton, &QToolButton::clicked, this, &DropdownEditorWindow::addNewTab);

    connect(m_settingsButton, &QToolButton::pressed, this, [this]() {
        m_suspendAutoHide = true;
    });
    connect(m_settingsButton, &QToolButton::clicked, this, [this]() {
        openSettings();
    });

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (m_stack && index >= 0 && index < m_stack->count()) {
            m_stack->setCurrentIndex(index);
            if (auto *editor = currentEditor()) {
                editor->setFocus();
            }
        }
    });

    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (!m_stack) {
            return;
        }
        QWidget *page = m_stack->widget(from);
        if (!page) {
            return;
        }
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
    });

    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int index = m_tabBar->tabAt(pos);
        if (index < 0) {
            return;
        }

        QMenu menu(this);
        QAction *closeAction = menu.addAction(QStringLiteral("Закрыть эту вкладку"));
        QAction *saveAction = menu.addAction(QStringLiteral("Сохранить в файл"));
        QAction *saveAsAction = menu.addAction(QStringLiteral("Сохранить как..."));
        QAction *openAction = menu.addAction(QStringLiteral("Открыть файл в вкладке"));

        QAction *chosen = menu.exec(m_tabBar->mapToGlobal(pos));
        if (chosen == closeAction) {
            closeTab(index);
        } else if (chosen == saveAction) {
            saveTabToFile(index, false);
        } else if (chosen == saveAsAction) {
            saveTabToFile(index, true);
        } else if (chosen == openAction) {
            openFileIntoTab(index);
        }
    });

    createInitialTab();

    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("toggle_dropdown_editor"));
    m_toggleAction->setText(QStringLiteral("Toggle Dropdown Editor"));
    m_toggleAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_toggleAction, &QAction::triggered, this, &DropdownEditorWindow::togglePanel);

    registerGlobalShortcut(m_settings.toggleShortcut);
    applySettings(m_settings);

    hidePanel();
}

bool DropdownEditorWindow::registerGlobalShortcut(const QKeySequence &seq)
{
    const QKeySequence shortcut = seq.isEmpty() ? QKeySequence(QStringLiteral("Alt+S")) : seq;
    const QList<QKeySequence> shortcuts{shortcut};

    const bool okDefault = KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, shortcuts);
    const bool okActive = KGlobalAccel::self()->setShortcut(m_toggleAction, shortcuts);

    return okDefault && okActive;
}

AppSettings DropdownEditorWindow::loadSettings() const
{
    QSettings s(QString::fromLatin1(kAppName));

    AppSettings out;
    out.fontPointSize = s.value(QStringLiteral("ui/fontPointSize"), 12).toInt();
    out.toggleShortcut = QKeySequence(s.value(QStringLiteral("ui/toggleShortcut"),
                                               QStringLiteral("Alt+S")).toString());
    out.widthPercent = s.value(QStringLiteral("window/widthPercent"), 100).toInt();
    out.heightPercent = s.value(QStringLiteral("window/heightPercent"), 40).toInt();
    out.cornerRadius = s.value(QStringLiteral("window/cornerRadius"), 14).toInt();
    out.opacityPercent = s.value(QStringLiteral("window/opacityPercent"), 100).toInt();

    const QColor bg(s.value(QStringLiteral("ui/backgroundColor"), QStringLiteral("#1f1f1f")).toString());
    const QColor fg(s.value(QStringLiteral("ui/textColor"), QStringLiteral("#f2f2f2")).toString());

    out.backgroundColor = bg.isValid() ? bg : QColor(QStringLiteral("#1f1f1f"));
    out.textColor = fg.isValid() ? fg : QColor(QStringLiteral("#f2f2f2"));

    return out;
}

void DropdownEditorWindow::saveSettings(const AppSettings &settings) const
{
    QSettings s(QString::fromLatin1(kAppName));

    s.setValue(QStringLiteral("ui/fontPointSize"), settings.fontPointSize);
    s.setValue(QStringLiteral("ui/toggleShortcut"), settings.toggleShortcut.toString());
    s.setValue(QStringLiteral("window/widthPercent"), settings.widthPercent);
    s.setValue(QStringLiteral("window/heightPercent"), settings.heightPercent);
    s.setValue(QStringLiteral("window/cornerRadius"), settings.cornerRadius);
    s.setValue(QStringLiteral("window/opacityPercent"), settings.opacityPercent);
    s.setValue(QStringLiteral("ui/backgroundColor"), settings.backgroundColor.name(QColor::HexRgb));
    s.setValue(QStringLiteral("ui/textColor"), settings.textColor.name());
    s.sync();
}

void DropdownEditorWindow::applySettings(const AppSettings &settings)
{
    m_settings = settings;

    registerGlobalShortcut(m_settings.toggleShortcut);
    applyWindowAppearance();
    updateExistingEditors();

    if (isVisible()) {
        repositionToTargetScreen();
    }
}

void DropdownEditorWindow::applyWindowAppearance()
{
    QColor bg = m_settings.backgroundColor;
    int alpha = m_settings.opacityPercent * 255 / 100;
    bg.setAlpha(alpha);
    m_settings.backgroundColor = bg;

    m_rootWidget->setStyleSheet(QStringLiteral(
        "#rootWidget {"
        "border-radius: %1px;"
        "background: transparent;"
        "}").arg(QString::number(m_settings.cornerRadius)));

    m_headerWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));

    update();
}

void DropdownEditorWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QPainterPath path;
    path.addRoundedRect(rect(), m_settings.cornerRadius, m_settings.cornerRadius);
    painter.fillPath(path, m_settings.backgroundColor);
}

void DropdownEditorWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    update();
}

void DropdownEditorWindow::createInitialTab()
{
    addNewTab();
}

QPlainTextEdit *DropdownEditorWindow::editorAt(int index) const
{
    if (!m_stack || index < 0 || index >= m_stack->count()) {
        return nullptr;
    }

    return qobject_cast<QPlainTextEdit *>(m_stack->widget(index));
}

QWidget *DropdownEditorWindow::pageAt(int index) const
{
    if (!m_stack || index < 0 || index >= m_stack->count()) {
        return nullptr;
    }

    return m_stack->widget(index);
}

QPlainTextEdit *DropdownEditorWindow::currentEditor() const
{
    return editorAt(m_tabBar ? m_tabBar->currentIndex() : -1);
}

void DropdownEditorWindow::addNewTab()
{
    auto *editor = new QPlainTextEdit(m_stack);
    editor->setFrameShape(QFrame::NoFrame);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setPlaceholderText(QStringLiteral("Новая вкладка"));
    editor->setTabStopDistance(4.0 * editor->fontMetrics().horizontalAdvance(QChar(' ')));
    editor->setProperty("filePath", QString());

    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor](bool) {
        updateTabTitleForEditor(editor);
    });

    applyEditorStyle(editor);

    const int index = m_stack->addWidget(editor);
    m_tabBar->addTab(QString());
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
    updateTabTitleForEditor(editor);
    editor->setFocus();
}

void DropdownEditorWindow::applyEditorStyle(QPlainTextEdit *editor) const
{
    if (!editor) {
        return;
    }

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(m_settings.fontPointSize);
    editor->setFont(font);

    QPalette pal = editor->palette();
    QColor baseColor = m_settings.backgroundColor;
    baseColor.setAlpha(255);
    pal.setColor(QPalette::Base, baseColor);
    pal.setColor(QPalette::Window, baseColor);
    pal.setColor(QPalette::Text, m_settings.textColor);
    pal.setColor(QPalette::WindowText, m_settings.textColor);
    pal.setColor(QPalette::PlaceholderText, m_settings.textColor.darker(140));
    editor->setPalette(pal);
    editor->setAutoFillBackground(true);
}

void DropdownEditorWindow::updateExistingEditors()
{
    if (!m_stack) {
        return;
    }

    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *editor = editorAt(i)) {
            applyEditorStyle(editor);
        }
    }

    applyWindowAppearance();
    updateAllTabTitles();
}

void DropdownEditorWindow::updateTabTitleForEditor(QPlainTextEdit *editor)
{
    if (!m_tabBar || !m_stack || !editor) {
        return;
    }

    const int index = m_stack->indexOf(editor);
    if (index < 0) {
        return;
    }

    const QString path = currentEditorFilePath(editor);
    QString title = path.isEmpty()
        ? QStringLiteral("Без имени %1").arg(index + 1)
        : QFileInfo(path).fileName();

    if (editor->document()->isModified()) {
        title += QStringLiteral("*");
    }

    m_tabBar->setTabText(index, title);
}

void DropdownEditorWindow::updateAllTabTitles()
{
    if (!m_stack) {
        return;
    }

    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *editor = editorAt(i)) {
            updateTabTitleForEditor(editor);
        }
    }
}

bool DropdownEditorWindow::openFileIntoEditor(QPlainTextEdit *editor, const QString &path)
{
    if (!editor) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось открыть файл:\n%1").arg(path));
        return false;
    }

    const QString text = QString::fromUtf8(file.readAll());
    editor->setPlainText(text);
    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    updateTabTitleForEditor(editor);
    return true;
}

bool DropdownEditorWindow::saveEditorToPath(QPlainTextEdit *editor, const QString &path)
{
    if (!editor) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось сохранить файл:\n%1").arg(path));
        return false;
    }

    const QByteArray data = editor->toPlainText().toUtf8();
    if (file.write(data) < 0) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось записать файл:\n%1").arg(path));
        return false;
    }

    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    updateTabTitleForEditor(editor);
    return true;
}

void DropdownEditorWindow::closeTab(int index)
{
    if (!m_stack || !m_tabBar || index < 0 || index >= m_stack->count()) {
        return;
    }

    QWidget *page = m_stack->widget(index);
    m_stack->removeWidget(page);
    m_tabBar->removeTab(index);

    if (page) {
        page->deleteLater();
    }

    if (m_stack->count() == 0) {
        createInitialTab();
    } else {
        const int newIndex = qMin(index, m_stack->count() - 1);
        m_stack->setCurrentIndex(newIndex);
        m_tabBar->setCurrentIndex(newIndex);
    }

    updateAllTabTitles();
}

void DropdownEditorWindow::saveTabToFile(int index, bool saveAs)
{
    QPlainTextEdit *editor = editorAt(index);
    if (!editor) {
        return;
    }

    QString path = currentEditorFilePath(editor);

    m_suspendAutoHide = true;

    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить файл"));
    }

    QTimer::singleShot(150, this, [this]() {
        m_suspendAutoHide = false;
    });

    if (path.isEmpty()) {
        return;
    }

    saveEditorToPath(editor, path);
}

void DropdownEditorWindow::openFileIntoTab(int index)
{
    QPlainTextEdit *editor = editorAt(index);
    if (!editor) {
        return;
    }

    m_suspendAutoHide = true;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Открыть файл"));
    QTimer::singleShot(150, this, [this]() {
        m_suspendAutoHide = false;
    });

    if (path.isEmpty()) {
        return;
    }

    openFileIntoEditor(editor, path);
}

QScreen *DropdownEditorWindow::targetScreen() const
{
    if (const QWindow *fw = QGuiApplication::focusWindow()) {
        if (fw->screen()) {
            return fw->screen();
        }
    }

    const QPoint cursorPos = QCursor::pos();
    if (QScreen *screen = QGuiApplication::screenAt(cursorPos)) {
        return screen;
    }

    return QGuiApplication::primaryScreen();
}

void DropdownEditorWindow::repositionToTargetScreen()
{
    QScreen *screen = targetScreen();
    if (!screen) {
        return;
    }

    const QRect geo = screen->availableGeometry();
    const int w = qBound(320, (geo.width() * m_settings.widthPercent) / 100, geo.width());
    const int h = qBound(180, (geo.height() * m_settings.heightPercent) / 100, geo.height());

    const int x = geo.x() + (geo.width() - w) / 2;
    const int y = geo.y() + (geo.height() - h) / 2;

    setGeometry(x, y, w, h);
}

void DropdownEditorWindow::showPanel()
{
    repositionToTargetScreen();
    show();
    raise();
    activateWindow();
    if (windowHandle()) {
        windowHandle()->requestActivate();
    }
}

void DropdownEditorWindow::hidePanel()
{
    hide();
}

void DropdownEditorWindow::togglePanel()
{
    if (isVisible()) {
        hidePanel();
    } else {
        showPanel();
    }
}

void DropdownEditorWindow::openSettings()
{
    if (m_settingsDialog) {
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
        return;
    }

    m_suspendAutoHide = true;

    auto *dialog = new SettingsDialog(m_settings, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    m_settingsDialog = dialog;

    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        auto *settingsDialog = static_cast<SettingsDialog *>(dialog);
        const AppSettings newSettings = settingsDialog->settings();
        saveSettings(newSettings);
        applySettings(newSettings);
    });

    connect(dialog, &QDialog::finished, this, [this](int) {
        m_settingsDialog = nullptr;
        QTimer::singleShot(150, this, [this]() {
            m_suspendAutoHide = false;
        });
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

bool DropdownEditorWindow::isOwnWindow(const QWidget *w) const
{
    if (!w) {
        return false;
    }

    return w == this || w == m_settingsDialog;
}

bool DropdownEditorWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        if (m_suspendAutoHide) {
            return QMainWindow::event(event);
        }

        QWidget *modal = QApplication::activeModalWidget();
        if (!modal || !isOwnWindow(modal)) {
            const QWidget *aw = QApplication::activeWindow();
            if (!isOwnWindow(aw)) {
                hidePanel();
            }
        }
    }

    return QMainWindow::event(event);
}

void DropdownEditorWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hidePanel();
}