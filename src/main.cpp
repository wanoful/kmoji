// SPDX-License-Identifier: MIT
#include "picker.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>

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
    QCommandLineParser parser;
    parser.setApplicationDescription("kMoji — native Fcitx5 emoji picker");
    parser.addHelpOption();
    parser.addOptions({{"preview", "Open a standalone UI preview"},
        {"socket", "Connect to the Fcitx5 addon", "path"},
        {"data", "Override the emoji catalogue", "path"},
        {"screenshot", "Save a preview PNG and exit", "path"},
        {"query", "Initial query for preview", "text"}});
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
        qCritical("Cannot load emoji catalogue: %s", qPrintable(dataPath));
        return 1;
    }
    if (preview) {
        picker.show();
        picker.setPreviewQuery(parser.value("query"));
        if (parser.isSet("screenshot")) QTimer::singleShot(700, [&] {
            bool ok = picker.grab().save(parser.value("screenshot"));
            app.exit(ok ? 0 : 1);
        });
    } else picker.connectToAddon(parser.value("socket"));
    return app.exec();
}
