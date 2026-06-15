#include <QApplication>
#include <QFont>
#include <QToolTip>
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
    app.setApplicationName(QStringLiteral("PixoBook"));
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("PixoBook");

    QFont appFont = app.font();
    appFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    appFont.setPointSize(10);
    app.setFont(appFont);
    QToolTip::setFont(appFont);

    Codicon::init();

    ParserRegistry::instance().registerParser(std::make_unique<SDParser>());
    ParserRegistry::instance().registerParser(std::make_unique<MJParser>());
    ParserRegistry::instance().registerParser(std::make_unique<DALLEParser>());

    auto *style = new CustomStyle();
    app.setStyle(style);

    app.setStyleSheet(QStringLiteral(
        "QToolTip { font-family: \"Microsoft YaHei UI\"; font-size: 10pt; color: #cccccc; background-color: #2d2d2d; border: 1px solid #404040; padding: 4px 8px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #2d2d2d; color: #cccccc;"
        "  border: 1px solid #3c3c3c; outline: none;"
        "  selection-background-color: #094771; selection-color: #cccccc;"
        "}"
        "QComboBox QAbstractItemView::item { height: 26px; padding: 0 12px; }"
    ));

    app.setPalette(style->standardPalette());

    MainWindow window;
    window.setWindowTitle(QStringLiteral("PixoBook"));
    window.resize(1600, 1000);
    window.show();

    return app.exec();
}
