#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <memory>
#include "ui/MainWindow.h"
#include "ui/CustomStyle.h"
#include "ui/Codicon.h"
#include "core/ParserRegistry.h"
#include "parsers/SDParser.h"
#include "parsers/MJParser.h"
#include "parsers/DALLEParser.h"

int main(int argc, char *argv[])
{
#if defined(Q_OS_WIN)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("AI素材库");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AIMaterialLibrary");

    QFont appFont = app.font();
    appFont.setPointSize(11);
    app.setFont(appFont);

    Codicon::init();

    // Register parsers
    ParserRegistry::instance().registerParser(std::make_unique<SDParser>());
    ParserRegistry::instance().registerParser(std::make_unique<MJParser>());
    ParserRegistry::instance().registerParser(std::make_unique<DALLEParser>());

    app.setStyle(new CustomStyle());

    MainWindow window;
    window.setWindowTitle("AI素材库");
    window.resize(1600, 1000);
    window.show();

    return app.exec();
}
