// SPDX-License-Identifier: MIT
#include "picker.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

int main(int argc, char **argv) {
    // The passive panel receives keys from the addon. Registering another Qt
    // input context here can reset the target application's IME focus on show.
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--socket") {
            qputenv("QT_IM_MODULE", "none");
            qputenv("QT_IM_MODULES", "none");
            break;
        }
    }
    QApplication app(argc, argv);
    app.setOrganizationName("kmoji");
    app.setApplicationName("kmoji");
    app.setApplicationDisplayName("kMoji");
    app.setQuitOnLastWindowClosed(false);
    const auto locale = QLocale::system();
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qtbase", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);
    QTranslator translator;
    if (translator.load(locale, "kmoji", "_", ":/i18n")) app.installTranslator(&translator);
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("Main", "kMoji — native Fcitx5 emoji and symbol picker"));
    parser.addHelpOption();
    parser.addOptions({{"preview", QCoreApplication::translate("Main", "Open a standalone UI preview")},
        {"symbols", QCoreApplication::translate("Main", "Preview the special symbols catalogue")},
        {"socket", QCoreApplication::translate("Main", "Connect to the Fcitx5 addon"), "path"},
        {"data", QCoreApplication::translate("Main", "Override the emoji catalogue"), "path"},
        {"screenshot", QCoreApplication::translate("Main", "Save a preview PNG and exit"), "path"},
        {"query", QCoreApplication::translate("Main", "Initial query for preview"), "text"}});
    parser.process(app);
    QString dataPath = parser.value("data");
    if (dataPath.isEmpty()) {
        dataPath = KMOJI_DATA_PATH;
        if (!QFileInfo::exists(dataPath)) dataPath = QApplication::applicationDirPath()+"/../data/emoji.json";
    }
    bool preview = parser.isSet("preview") || parser.isSet("screenshot");
    if (!preview && !parser.isSet("socket")) parser.showHelp();
    Picker picker(dataPath, preview);
    if (!picker.catalogueLoaded()) {
        qCritical("%s", qPrintable(QCoreApplication::translate("Main", "Cannot load the emoji catalogue: %1").arg(dataPath)));
        return 1;
    }
    if (preview) {
        if (parser.isSet("symbols")) picker.setSymbolMode(true);
        picker.show();
        picker.setPreviewQuery(parser.value("query"));
        if (parser.isSet("screenshot")) QTimer::singleShot(700, [&] {
            bool ok = picker.grab().save(parser.value("screenshot"));
            app.exit(ok ? 0 : 1);
        });
    } else picker.connectToAddon(parser.value("socket"));
    return app.exec();
}
