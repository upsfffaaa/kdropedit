#include <QApplication>
#include "kdropedit.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("upsfffaaa"));
    app.setApplicationName(QStringLiteral("kdropedit"));
    app.setQuitOnLastWindowClosed(false);

    DropdownEditorWindow window;
    return app.exec();
}