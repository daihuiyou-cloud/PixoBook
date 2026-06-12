#include <QApplication>
#include <QStyleFactory>
#include "ui/MainWindow.h"
#include "ui/CustomStyle.h"
#include "ui/Codicon.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AI素材库");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AIMaterialLibrary");

    Codicon::init();

    app.setStyle(new CustomStyle());

    MainWindow window;
    window.setWindowTitle("AI素材库");
    window.resize(1280, 800);
    window.show();

    return app.exec();
}
