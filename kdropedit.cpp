#include "kdropedit.h"
#include "settings.h"
#include <KGlobalAccel>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
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
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QTabBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr auto kAppName = "kdropedit";

enum class SyntaxMode
{
    Plain,
    Markdown,
    CLike,
    Json
};

static SyntaxMode syntaxModeForPath(const QString &path)
{
    if (path.isEmpty()) {
        return SyntaxMode::Plain;
    }
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("md") || ext == QStringLiteral("markdown") || ext == QStringLiteral("mdx")) {
        return SyntaxMode::Markdown;
    }
    if (ext == QStringLiteral("json")) {
        return SyntaxMode::Json;
    }
    if (ext == QStringLiteral("c") || ext == QStringLiteral("h") || ext == QStringLiteral("cpp")
        || ext == QStringLiteral("hpp") || ext == QStringLiteral("cc") || ext == QStringLiteral("hh")
        || ext == QStringLiteral("cxx") || ext == QStringLiteral("hxx") || ext == QStringLiteral("js")
        || ext == QStringLiteral("ts") || ext == QStringLiteral("java") || ext == QStringLiteral("cs")
        || ext == QStringLiteral("go") || ext == QStringLiteral("rs") || ext == QStringLiteral("swift")
        || ext == QStringLiteral("m") || ext == QStringLiteral("mm")) {
        return SyntaxMode::CLike;
    }
    return SyntaxMode::Plain;
}

class DragHandleWidget final : public QWidget
{
public:
    explicit DragHandleWidget(QWidget *windowToMove, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_window(windowToMove)
    {
        setFixedSize(48, 28);
        setCursor(Qt::SizeAllCursor);
        setToolTip(tr("Move window"));
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

class SimpleSyntaxHighlighter final : public QSyntaxHighlighter
{
public:
    explicit SimpleSyntaxHighlighter(QTextDocument *document)
        : QSyntaxHighlighter(document)
    {
        m_headingFormat.setForeground(QColor(QStringLiteral("#7dd3fc")));
        m_headingFormat.setFontWeight(QFont::Bold);
        m_listFormat.setForeground(QColor(QStringLiteral("#a5b4fc")));
        m_boldFormat.setForeground(QColor(QStringLiteral("#f9a8d4")));
        m_boldFormat.setFontWeight(QFont::Bold);
        m_italicFormat.setForeground(QColor(QStringLiteral("#f9a8d4")));
        m_italicFormat.setFontItalic(true);
        m_codeFormat.setForeground(QColor(QStringLiteral("#fbbf24")));
        m_codeFormat.setFontFamilies(QStringList{QStringLiteral("monospace")});
        m_linkFormat.setForeground(QColor(QStringLiteral("#86efac")));
        m_linkFormat.setFontUnderline(true);
        m_commentFormat.setForeground(QColor(QStringLiteral("#86efac")));
        m_stringFormat.setForeground(QColor(QStringLiteral("#fbbf24")));
        m_numberFormat.setForeground(QColor(QStringLiteral("#f59e0b")));
        m_keywordFormat.setForeground(QColor(QStringLiteral("#7dd3fc")));
        m_keywordFormat.setFontWeight(QFont::Bold);
        m_typeFormat.setForeground(QColor(QStringLiteral("#c084fc")));
        m_typeFormat.setFontWeight(QFont::Bold);
        m_jsonKeyFormat.setForeground(QColor(QStringLiteral("#7dd3fc")));
        m_jsonKeyFormat.setFontWeight(QFont::Bold);
    }
    void setMode(SyntaxMode mode)
    {
        if (m_mode == mode) {
            return;
        }
        m_mode = mode;
        rehighlight();
    }
protected:
    void highlightBlock(const QString &text) override
    {
        switch (m_mode) {
        case SyntaxMode::Markdown:
            highlightMarkdown(text);
            break;
        case SyntaxMode::Json:
            highlightJson(text);
            break;
        case SyntaxMode::CLike:
            highlightCLike(text);
            break;
        case SyntaxMode::Plain:
        default:
            break;
        }
    }
private:
    void highlightMarkdown(const QString &text)
    {
        setCurrentBlockState(0);
        const QString trimmed = text.trimmed();
        if (trimmed.startsWith(QStringLiteral("```"))) {
            setFormat(0, text.size(), m_codeFormat);
            setCurrentBlockState(1);
            return;
        }
        if (previousBlockState() == 1) {
            setFormat(0, text.size(), m_codeFormat);
            if (trimmed.startsWith(QStringLiteral("```"))) {
                setCurrentBlockState(0);
            } else {
                setCurrentBlockState(1);
            }
            return;
        }
        auto apply = [this, &text](const QRegularExpression &rx, const QTextCharFormat &fmt) {
            auto it = rx.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                setFormat(m.capturedStart(), m.capturedLength(), fmt);
            }
        };
        apply(QRegularExpression(QStringLiteral(R"(^#{1,6}\s+.*$)"), QRegularExpression::MultilineOption), m_headingFormat);
        apply(QRegularExpression(QStringLiteral(R"(^\s*([-*+]\s+|\d+\.\s+).*$)"), QRegularExpression::MultilineOption), m_listFormat);
        apply(QRegularExpression(QStringLiteral(R"(\*\*[^*\n]+\*\*|__[^_\n]+__)")), m_boldFormat);
        apply(QRegularExpression(QStringLiteral(R"((?<!\*)\*[^*\n]+\*(?!\*)|(?<!_)_[^_\n]+_(?!_))")), m_italicFormat);
        apply(QRegularExpression(QStringLiteral(R"(`[^`]+`)")), m_codeFormat);
        apply(QRegularExpression(QStringLiteral(R"(\[[^\]]+\]\([^)]+\))")), m_linkFormat);
        apply(QRegularExpression(QStringLiteral(R"(^>\s+.*$)"), QRegularExpression::MultilineOption), m_commentFormat);
    }
    void highlightJson(const QString &text)
    {
        applyRegex(text, QRegularExpression(QStringLiteral(R"("([^"\\]|\\.)*")")), m_stringFormat);
        applyRegex(text, QRegularExpression(QStringLiteral(R"(\b(true|false|null)\b)")), m_keywordFormat);
        applyRegex(text, QRegularExpression(QStringLiteral(R"(\b-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?\b)")), m_numberFormat);
        auto it = QRegularExpression(QStringLiteral(R"("([^"\\]|\\.)*"\s*:)")).globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), m_jsonKeyFormat);
        }
    }
    void highlightCLike(const QString &text)
    {
        highlightMultilineComments(text);
        applyRegex(text, QRegularExpression(QStringLiteral(R"("([^"\\]|\\.)*")")), m_stringFormat);
        applyRegex(text, QRegularExpression(QStringLiteral(R"('([^'\\]|\\.)*')")), m_stringFormat);
        applyRegex(text, QRegularExpression(QStringLiteral(R"(\b\d+(\.\d+)?\b)")), m_numberFormat);
        static const QStringList keywords = {
            QStringLiteral("auto"), QStringLiteral("bool"), QStringLiteral("break"), QStringLiteral("case"),
            QStringLiteral("catch"), QStringLiteral("char"), QStringLiteral("class"), QStringLiteral("const"),
            QStringLiteral("constexpr"), QStringLiteral("continue"), QStringLiteral("default"),
            QStringLiteral("delete"), QStringLiteral("do"), QStringLiteral("double"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("explicit"), QStringLiteral("extern"), QStringLiteral("false"),
            QStringLiteral("final"), QStringLiteral("float"), QStringLiteral("for"), QStringLiteral("friend"),
            QStringLiteral("if"), QStringLiteral("inline"), QStringLiteral("int"), QStringLiteral("long"),
            QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("nullptr"), QStringLiteral("operator"),
            QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("return"),
            QStringLiteral("short"), QStringLiteral("signed"), QStringLiteral("sizeof"), QStringLiteral("static"),
            QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
            QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
            QStringLiteral("typename"), QStringLiteral("union"), QStringLiteral("unsigned"), QStringLiteral("using"),
            QStringLiteral("virtual"), QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
            QStringLiteral("let"), QStringLiteral("var"), QStringLiteral("function"),
            QStringLiteral("import"), QStringLiteral("export"), QStringLiteral("yield"), QStringLiteral("await")
        };
        for (const QString &kw : keywords) {
            applyRegex(text, QRegularExpression(QStringLiteral(R"(\b%1\b)").arg(QRegularExpression::escape(kw))), m_keywordFormat);
        }
        static const QStringList types = {
            QStringLiteral("QString"), QStringLiteral("QByteArray"), QStringLiteral("QVector"),
            QStringLiteral("QList"), QStringLiteral("QMap"), QStringLiteral("QHash"),
            QStringLiteral("std::string"), QStringLiteral("size_t"), QStringLiteral("int32_t"),
            QStringLiteral("int64_t"), QStringLiteral("uint32_t"), QStringLiteral("uint64_t")
        };
        for (const QString &type : types) {
            applyRegex(text, QRegularExpression(QStringLiteral(R"(\b%1\b)").arg(QRegularExpression::escape(type))), m_typeFormat);
        }
    }
    void highlightMultilineComments(const QString &text)
    {
        int startIndex = 0;
        if (previousBlockState() != 1) {
            startIndex = text.indexOf(QStringLiteral("/*"));
        }
        while (startIndex >= 0) {
            const int endIndex = text.indexOf(QStringLiteral("*/"), startIndex + 2);
            if (endIndex == -1) {
                setCurrentBlockState(1);
                setFormat(startIndex, text.length() - startIndex, m_commentFormat);
                return;
            }
            const int commentLength = endIndex - startIndex + 2;
            setFormat(startIndex, commentLength, m_commentFormat);
            startIndex = text.indexOf(QStringLiteral("/*"), endIndex + 2);
        }
        if (previousBlockState() == 1) {
            const int endIndex = text.indexOf(QStringLiteral("*/"));
            if (endIndex == -1) {
                setCurrentBlockState(1);
                setFormat(0, text.length(), m_commentFormat);
            } else {
                setCurrentBlockState(0);
                setFormat(0, endIndex + 2, m_commentFormat);
            }
        }
        const int lineCommentIndex = text.indexOf(QStringLiteral("//"));
        if (lineCommentIndex >= 0) {
            setFormat(lineCommentIndex, text.length() - lineCommentIndex, m_commentFormat);
        }
    }
    void applyRegex(const QString &text, const QRegularExpression &rx, const QTextCharFormat &fmt)
    {
        auto it = rx.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), fmt);
        }
    }
private:
    SyntaxMode m_mode = SyntaxMode::Plain;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_listFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_jsonKeyFormat;
};

SimpleSyntaxHighlighter *syntaxHighlighterForDocument(QTextDocument *document)
{
    if (!document) {
        return nullptr;
    }
    const auto children = document->children();
    for (QObject *child : children) {
        if (auto *highlighter = dynamic_cast<SimpleSyntaxHighlighter *>(child)) {
            return highlighter;
        }
    }
    return nullptr;
}

static QString currentEditorFilePath(QPlainTextEdit *editor)
{
    return editor ? editor->property("filePath").toString() : QString();
}
}

DropdownEditorWindow::DropdownEditorWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(loadSettings())
{
    if (qApp) {
        qApp->setQuitOnLastWindowClosed(false);
    }
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
    m_tabBar->setTabsClosable(true);
    m_dragHandle = new DragHandleWidget(this, m_headerWidget);
    m_settingsButton = new QToolButton(m_headerWidget);
    m_settingsButton->setText(QStringLiteral("⚙"));
    m_settingsButton->setAutoRaise(true);
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setFixedSize(28, 28);
    headerLayout->addWidget(m_addTabButton, 0, Qt::AlignVCenter);
    headerLayout->addWidget(m_tabBar, 1);
    headerLayout->addWidget(m_dragHandle, 0, Qt::AlignVCenter);
    headerLayout->addWidget(m_settingsButton, 0, Qt::AlignVCenter);

    auto *searchWidget = new QWidget(m_rootWidget);
    m_searchWidget = searchWidget;
    auto *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(8, 4, 8, 4);
    searchLayout->setSpacing(6);
    m_searchField = new QLineEdit(searchWidget);
    m_searchField->setClearButtonEnabled(true);
    m_replaceField = new QLineEdit(searchWidget);
    m_replaceField->setClearButtonEnabled(true);
    m_replaceButton = new QToolButton(searchWidget);
    m_replaceButton->setCursor(Qt::PointingHandCursor);
    m_replaceAllButton = new QToolButton(searchWidget);
    m_replaceAllButton->setCursor(Qt::PointingHandCursor);
    searchLayout->addWidget(m_searchField, 2);
    searchLayout->addWidget(m_replaceField, 2);
    searchLayout->addWidget(m_replaceButton, 0);
    searchLayout->addWidget(m_replaceAllButton, 0);
    auto *headerLine = new QFrame(m_rootWidget);
    headerLine->setFrameShape(QFrame::HLine);
    headerLine->setFrameShadow(QFrame::Sunken);
    headerLine->setStyleSheet(QStringLiteral("background: rgba(127,127,127,0.4);"));
    m_stack = new QStackedWidget(m_rootWidget);
    m_stack->setAttribute(Qt::WA_StyledBackground, false);
    m_stack->setAutoFillBackground(false);
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));
    rootLayout->addWidget(m_headerWidget);
    rootLayout->addWidget(searchWidget);
    rootLayout->addWidget(headerLine);
    rootLayout->addWidget(m_stack, 1);
    setCentralWidget(m_rootWidget);

    connect(m_addTabButton, &QToolButton::clicked, this, &DropdownEditorWindow::addNewTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &DropdownEditorWindow::closeTab);
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
                updateSearchHighlights();
            }
        }
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (!m_stack) return;
        QWidget *page = m_stack->widget(from);
        if (!page) return;
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
        updateAllTabTitles();
        updateSearchHighlights();
    });
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int index = m_tabBar->tabAt(pos);
        if (index < 0) return;
        QMenu menu(this);
        QAction *closeAction = menu.addAction(tr("Close this tab"));
        QAction *saveAction = menu.addAction(tr("Save to file"));
        QAction *saveAsAction = menu.addAction(tr("Save as..."));
        QAction *openAction = menu.addAction(tr("Open file in tab"));
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
    connect(m_searchField, &QLineEdit::textChanged, this, [this]() {
        updateSearchHighlights();
    });
    connect(m_searchField, &QLineEdit::returnPressed, this, [this]() {
        findNextMatch();
    });
    connect(m_replaceField, &QLineEdit::returnPressed, this, [this]() {
        replaceCurrentMatch();
    });
    connect(m_replaceButton, &QToolButton::clicked, this, [this]() {
        replaceCurrentMatch();
    });
    connect(m_replaceAllButton, &QToolButton::clicked, this, [this]() {
        replaceAllMatches();
    });
    createInitialTab();
    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("toggle_dropdown_editor"));
    m_toggleAction->setText(tr("Toggle Dropdown Editor"));
    m_toggleAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_toggleAction, &QAction::triggered, this, &DropdownEditorWindow::togglePanel);
    registerGlobalShortcut(m_settings.toggleShortcut);
    applySettings(m_settings);
    retranslateUi();
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
    out.language = s.value(QStringLiteral("ui/language"), 0).toInt();
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
    s.setValue(QStringLiteral("ui/language"), settings.language);
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
    const int alpha = m_settings.opacityPercent * 255 / 100;
    m_windowBackgroundColor = m_settings.backgroundColor;
    m_windowBackgroundColor.setAlpha(alpha);
    m_rootWidget->setStyleSheet(QStringLiteral(
                                    "#rootWidget {"
                                    "border-radius: %1px;"
                                    "background: transparent;"
                                    "}").arg(QString::number(m_settings.cornerRadius)));
    m_headerWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    if (m_searchWidget) {
        m_searchWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    }
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
    painter.fillPath(path, m_windowBackgroundColor);
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
    editor->setTabStopDistance(4.0 * editor->fontMetrics().horizontalAdvance(QChar(' ')));
    editor->setProperty("filePath", QString());
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor](bool) {
        updateTabTitleForEditor(editor);
    });

    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
        if (editor == currentEditor()) {
            updateSearchHighlights();
        }

        if (editor->document()->isEmpty()) {
            bool ok = false;
            const int pt = editor->property("fontPointSize").toInt(&ok);
            if (ok && pt > 0) {
                QTextCharFormat fmt;
                fmt.setFontPointSize(pt);
                editor->setCurrentCharFormat(fmt);
            }
        }
    });
    applyEditorStyle(editor);
    const int index = m_stack->addWidget(editor);
    m_tabBar->addTab(QString());
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
    updateTabTitleForEditor(editor);
    editor->setFocus();
    updateSearchHighlights();
}

void DropdownEditorWindow::applyEditorStyle(QPlainTextEdit *editor) const
{
    if (!editor) return;

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(-1);
    font.setPointSize(m_settings.fontPointSize);

    editor->document()->setDefaultFont(font);
    editor->setFont(font);

    QTextCharFormat sizeOnly;
    sizeOnly.setFontPointSize(m_settings.fontPointSize);
    QTextCursor cursor(editor->document());
    cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(sizeOnly);


    QTextCharFormat currentFmt;
    currentFmt.setFontPointSize(m_settings.fontPointSize);
    editor->setCurrentCharFormat(currentFmt);

    editor->setProperty("fontPointSize", m_settings.fontPointSize);

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

    updateSyntaxHighlightingForEditor(editor);
}

void DropdownEditorWindow::updateSyntaxHighlightingForEditor(QPlainTextEdit *editor) const
{
    if (!editor) return;
    const QString path = currentEditorFilePath(editor);
    const SyntaxMode mode = syntaxModeForPath(path);
    SimpleSyntaxHighlighter *highlighter = syntaxHighlighterForDocument(editor->document());
    if (!highlighter) {
        highlighter = new SimpleSyntaxHighlighter(editor->document());
    }
    highlighter->setMode(mode);
}

void DropdownEditorWindow::updateExistingEditors()
{
    if (!m_stack) return;
    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *editor = editorAt(i)) {
            applyEditorStyle(editor);
        }
    }
    applyWindowAppearance();
    updateAllTabTitles();
    updateSearchHighlights();
}

void DropdownEditorWindow::updateTabTitleForEditor(QPlainTextEdit *editor)
{
    if (!m_tabBar || !m_stack || !editor) return;
    const int index = m_stack->indexOf(editor);
    if (index < 0) return;
    const QString path = currentEditorFilePath(editor);
    QString title = path.isEmpty()
                        ? tr("Untitled %1").arg(index + 1)
                        : QFileInfo(path).fileName();
    if (editor->document()->isModified()) {
        title += QStringLiteral("*");
    }
    m_tabBar->setTabText(index, title);
}

void DropdownEditorWindow::updateAllTabTitles()
{
    if (!m_stack) return;
    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *editor = editorAt(i)) {
            updateTabTitleForEditor(editor);
        }
    }
}

bool DropdownEditorWindow::openFileIntoEditor(QPlainTextEdit *editor, const QString &path)
{
    if (!editor) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not open file:\n%1").arg(path));
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll());
    editor->setPlainText(text);
    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    updateSyntaxHighlightingForEditor(editor);
    updateTabTitleForEditor(editor);
    updateSearchHighlights();
    return true;
}

bool DropdownEditorWindow::saveEditorToPath(QPlainTextEdit *editor, const QString &path)
{
    if (!editor) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not save file:\n%1").arg(path));
        return false;
    }
    const QByteArray data = editor->toPlainText().toUtf8();
    if (file.write(data) < 0) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Could not write file:\n%1").arg(path));
        return false;
    }
    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    updateSyntaxHighlightingForEditor(editor);
    updateTabTitleForEditor(editor);
    return true;
}

void DropdownEditorWindow::closeTab(int index)
{
    if (!m_stack || !m_tabBar || index < 0 || index >= m_stack->count()) return;
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
    updateSearchHighlights();
}

void DropdownEditorWindow::closeCurrentTab()
{
    if (!m_tabBar) return;
    closeTab(m_tabBar->currentIndex());
}

void DropdownEditorWindow::saveTabToFile(int index, bool saveAs)
{
    QPlainTextEdit *editor = editorAt(index);
    if (!editor) return;
    QString path = currentEditorFilePath(editor);
    m_suspendAutoHide = true;
    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save file"));
    }
    QTimer::singleShot(150, this, [this]() {
        m_suspendAutoHide = false;
    });
    if (path.isEmpty()) return;
    saveEditorToPath(editor, path);
}

void DropdownEditorWindow::openFileIntoTab(int index)
{
    QPlainTextEdit *editor = editorAt(index);
    if (!editor) return;
    m_suspendAutoHide = true;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open file"));
    QTimer::singleShot(150, this, [this]() {
        m_suspendAutoHide = false;
    });
    if (path.isEmpty()) return;
    openFileIntoEditor(editor, path);
}

QScreen *DropdownEditorWindow::targetScreen() const
{
    const QPoint cursorPos = QCursor::pos();
    if (QScreen *screen = QGuiApplication::screenAt(cursorPos)) {
        return screen;
    }
    if (const QWindow *fw = QGuiApplication::focusWindow()) {
        if (fw->screen()) return fw->screen();
    }
    return QGuiApplication::primaryScreen();
}

void DropdownEditorWindow::repositionToTargetScreen()
{
    QScreen *screen = targetScreen();
    if (!screen) return;
    const QRect geo = screen->availableGeometry();
    const int w = qBound(320, (geo.width() * m_settings.widthPercent) / 100, geo.width());
    const int h = qBound(180, (geo.height() * m_settings.heightPercent) / 100, geo.height());
    const int x = geo.x() + (geo.width() - w) / 2;
    const int y = geo.y() + (geo.height() - h) / 2;
    setGeometry(x, y, w, h);
}

void DropdownEditorWindow::showPanel()
{
    show();
    repositionToTargetScreen();
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
    connect(dialog, &SettingsDialog::languageChanged, this, &DropdownEditorWindow::promptRestartForLanguage);
    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        auto *settingsDialog = static_cast<SettingsDialog *>(dialog);
        AppSettings newSettings = settingsDialog->settings();
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
    if (!w) return false;
    return w == this || w == m_settingsDialog;
}

void DropdownEditorWindow::promptRestartForLanguage(int newLanguage)
{
    if (newLanguage == m_settings.language)
        return;
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setWindowTitle(tr("Restart required"));
    msgBox.setText(tr("The language change will take effect after restart. Restart now?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    if (msgBox.exec() == QMessageBox::Yes) {
        AppSettings s = m_settings;
        s.language = newLanguage;
        saveSettings(s);
        QProcess::startDetached(QApplication::applicationFilePath(), QStringList(), QDir::currentPath());
        QApplication::quit();
    }
}

void DropdownEditorWindow::retranslateUi()
{
    setWindowTitle(tr("Text Editor"));
    m_addTabButton->setToolTip(tr("New tab"));
    m_settingsButton->setToolTip(tr("Settings"));
    m_searchField->setPlaceholderText(tr("Search in current tab"));
    m_replaceField->setPlaceholderText(tr("Replace"));
    m_replaceButton->setText(tr("Replace"));
    m_replaceButton->setToolTip(tr("Replace current match"));
    m_replaceAllButton->setText(tr("All"));
    m_replaceAllButton->setToolTip(tr("Replace all matches"));
    updateAllTabTitles();
}

void DropdownEditorWindow::updateSearchHighlights()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    const QString query = m_searchField ? m_searchField->text() : QString();
    if (query.isEmpty()) {
        editor->setExtraSelections({});
        return;
    }
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat fmt;
    fmt.setBackground(QColor(255, 230, 120, 85));
    fmt.setForeground(editor->palette().color(QPalette::Text));
    const QString text = editor->toPlainText();
    const QRegularExpression rx(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);
    auto it = rx.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        QTextCursor cursor(editor->document());
        cursor.setPosition(m.capturedStart());
        cursor.setPosition(m.capturedStart() + m.capturedLength(), QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection sel;
        sel.cursor = cursor;
        sel.format = fmt;
        selections.push_back(sel);
    }
    editor->setExtraSelections(selections);
}

void DropdownEditorWindow::findNextMatch()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    const QString query = m_searchField ? m_searchField->text() : QString();
    if (query.isEmpty()) return;
    const QRegularExpression rx(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);
    QTextCursor cursor = editor->textCursor();
    cursor = editor->document()->find(rx, cursor);
    if (cursor.isNull()) {
        QTextCursor start(editor->document());
        cursor = editor->document()->find(rx, start);
    }
    if (!cursor.isNull()) {
        editor->setTextCursor(cursor);
        editor->ensureCursorVisible();
    }
    updateSearchHighlights();
}

void DropdownEditorWindow::findPreviousMatch()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    const QString query = m_searchField ? m_searchField->text() : QString();
    if (query.isEmpty()) return;
    const QRegularExpression rx(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);
    QTextCursor cursor = editor->textCursor();
    cursor = editor->document()->find(rx, cursor, QTextDocument::FindBackward);
    if (cursor.isNull()) {
        QTextCursor end(editor->document());
        end.movePosition(QTextCursor::End);
        cursor = editor->document()->find(rx, end, QTextDocument::FindBackward);
    }
    if (!cursor.isNull()) {
        editor->setTextCursor(cursor);
        editor->ensureCursorVisible();
    }
    updateSearchHighlights();
}

void DropdownEditorWindow::replaceCurrentMatch()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    const QString query = m_searchField ? m_searchField->text() : QString();
    if (query.isEmpty()) return;
    const QString replacement = m_replaceField ? m_replaceField->text() : QString();
    QTextCursor cursor = editor->textCursor();
    const QString selected = cursor.selectedText();
    if (!selected.isEmpty() && selected.compare(query, Qt::CaseInsensitive) == 0) {
        cursor.insertText(replacement);
        editor->setTextCursor(cursor);
        editor->document()->setModified(true);
        updateTabTitleForEditor(editor);
        updateSearchHighlights();
        return;
    }
    findNextMatch();
    cursor = editor->textCursor();
    if (cursor.hasSelection() && cursor.selectedText().compare(query, Qt::CaseInsensitive) == 0) {
        cursor.insertText(replacement);
        editor->setTextCursor(cursor);
        editor->document()->setModified(true);
        updateTabTitleForEditor(editor);
    }
    updateSearchHighlights();
}

void DropdownEditorWindow::replaceAllMatches()
{
    QPlainTextEdit *editor = currentEditor();
    if (!editor) return;
    const QString query = m_searchField ? m_searchField->text() : QString();
    if (query.isEmpty()) return;
    const QString replacement = m_replaceField ? m_replaceField->text() : QString();
    const QString original = editor->toPlainText();
    QString updated = original;
    const QRegularExpression rx(QRegularExpression::escape(query), QRegularExpression::CaseInsensitiveOption);
    updated.replace(rx, replacement);
    if (updated == original) return;
    const int cursorPos = editor->textCursor().position();
    editor->setPlainText(updated);
    editor->document()->setModified(true);
    updateTabTitleForEditor(editor);
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(qMin(cursorPos, updated.size()));
    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
    updateSearchHighlights();
}

bool DropdownEditorWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        if (m_suspendAutoHide) return QMainWindow::event(event);
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