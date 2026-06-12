#include <QApplication>
#include <QFont>
#include <QFile>
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
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AI 素材库"));
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("AIMaterialLibrary");

    QFont appFont = app.font();
    appFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    appFont.setPointSize(10);
    app.setFont(appFont);

    Codicon::init();

    ParserRegistry::instance().registerParser(std::make_unique<SDParser>());
    ParserRegistry::instance().registerParser(std::make_unique<MJParser>());
    ParserRegistry::instance().registerParser(std::make_unique<DALLEParser>());

    app.setStyle(new CustomStyle());

    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    MainWindow window;
    window.setWindowTitle(QStringLiteral("AI 素材库"));
    window.resize(1600, 1000);
    window.show();

    return app.exec();
}
