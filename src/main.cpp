// SPDX-License-Identifier: MIT
#include "picker.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

namespace {
QString optionValue(int argc, char **argv, const QString &name) {
    const QByteArray flag = "--" + name.toUtf8();
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == flag && i+1 < argc) return QString::fromLocal8Bit(argv[i+1]);
        if (arg.startsWith(flag + "=")) return QString::fromLocal8Bit(arg.mid(flag.size()+1));
    }
    return {};
}
}

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
    QString language = qEnvironmentVariable("KMOJI_LANG");
    const auto cliLanguage = optionValue(argc, argv, "lang");
    if (!cliLanguage.isEmpty()) language = cliLanguage;
    const auto locale = language.isEmpty() ? QLocale::system() : QLocale(language);
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
        {"query", QCoreApplication::translate("Main", "Initial query for preview"), "text"},
        {"lang", QCoreApplication::translate("Main", "Override the UI language"), "code"},
        {"theme", QCoreApplication::translate("Main", "Override the UI theme (system, light, dark)"), "name"},
        {"accent", QCoreApplication::translate("Main", "Override the accent color"), "color"},
        {"hover-langs", QCoreApplication::translate("Main",
            "Languages shown in the hover label (comma-separated: en, zh)"), "list"},
        {"search-langs", QCoreApplication::translate("Main",
            "Additional emoji search languages (comma-separated: zh, pinyin)"), "list"}});
    parser.process(app);
    QString dataPath = parser.value("data");
    if (dataPath.isEmpty()) {
        dataPath = KMOJI_DATA_PATH;
        if (!QFileInfo::exists(dataPath)) dataPath = QApplication::applicationDirPath()+"/../data/emoji.json";
    }
    bool preview = parser.isSet("preview") || parser.isSet("screenshot");
    if (!preview && !parser.isSet("socket")) parser.showHelp();
    QStringList searchLanguages;
    if (parser.isSet("search-langs"))
        searchLanguages = parser.value("search-langs").split(',', Qt::SkipEmptyParts);
    QStringList hoverLanguages;
    if (parser.isSet("hover-langs"))
        hoverLanguages = parser.value("hover-langs").split(',', Qt::SkipEmptyParts);
    Picker picker(dataPath, preview, searchLanguages,
                  parser.value("theme"), parser.value("accent"), hoverLanguages);
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
