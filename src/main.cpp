#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include "ui/MainWindow.h"
#include "ui/CustomStyle.h"
#include "ui/Codicon.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_WIN)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("AI素材库");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AIMaterialLibrary");

    QFont appFont("Segoe UI", 10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);

    Codicon::init();

    app.setStyle(new CustomStyle());

    MainWindow window;
    window.setWindowTitle("AI素材库");
    window.resize(1600, 1000);
    window.show();

    return app.exec();
}
