#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QIcon>
#include "kdropedit.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName(QStringLiteral("upsfffaaa"));
    app.setApplicationName(QStringLiteral("kdropedit"));

    app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    QSettings s(QStringLiteral("kdropedit"));
    int lang = s.value(QStringLiteral("ui/language"), 0).toInt();

    QTranslator translator;
    QString qmFile = (lang == 1)
                         ? QStringLiteral("app_ru")
                         : QStringLiteral("app_en");

    if (translator.load(qmFile, QStringLiteral(":/translations")) ||
        translator.load(qmFile, QApplication::applicationDirPath() + QStringLiteral("/translations")) ||
        translator.load(qmFile))
    {
        app.installTranslator(&translator);
    }

    DropdownEditorWindow window;

    window.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    window.show();

    return app.exec();
}