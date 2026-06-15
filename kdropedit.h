#pragma once

#include <QColor>
#include <QKeySequence>
#include <QMainWindow>
#include <QPointer>

class QAction;
class QCloseEvent;
class QDialog;
class QPlainTextEdit;
class QScreen;
class QTabBar;
class QStackedWidget;
class QEvent;
class QToolButton;
class QResizeEvent;

struct AppSettings
{
    int fontPointSize = 12;
    QKeySequence toggleShortcut = QKeySequence(QStringLiteral("Alt+S"));
    int widthPercent = 100;
    int heightPercent = 40;
    int cornerRadius = 14;
    int opacityPercent = 100;
    QColor backgroundColor = QColor(QStringLiteral("#1f1f1f"));
    QColor textColor = QColor(QStringLiteral("#f2f2f2"));
};

class DropdownEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit DropdownEditorWindow(QWidget *parent = nullptr);

public slots:
    void togglePanel();
    void showPanel();
    void hidePanel();
    void openSettings();
    void addNewTab();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    AppSettings loadSettings() const;
    void saveSettings(const AppSettings &settings) const;
    void applySettings(const AppSettings &settings);
    bool registerGlobalShortcut(const QKeySequence &seq);

    QScreen *targetScreen() const;
    void repositionToTargetScreen();

    void updateExistingEditors();
    void applyEditorStyle(QPlainTextEdit *editor) const;
    void applyWindowAppearance();

    void createInitialTab();
    QPlainTextEdit *currentEditor() const;
    QPlainTextEdit *editorAt(int index) const;
    QWidget *pageAt(int index) const;

    void updateTabTitleForEditor(QPlainTextEdit *editor);
    void updateAllTabTitles();

    bool openFileIntoEditor(QPlainTextEdit *editor, const QString &path);
    bool saveEditorToPath(QPlainTextEdit *editor, const QString &path);

    void closeTab(int index);
    void saveTabToFile(int index, bool saveAs = false);
    void openFileIntoTab(int index);

    bool isOwnWindow(const QWidget *w) const;

private:
    AppSettings m_settings;

    QWidget *m_rootWidget = nullptr;
    QWidget *m_headerWidget = nullptr;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;

    QAction *m_toggleAction = nullptr;

    QPointer<QDialog> m_settingsDialog;
    QToolButton *m_addTabButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QWidget *m_dragHandle = nullptr;

    bool m_suspendAutoHide = false;
};