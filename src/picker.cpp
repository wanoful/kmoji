// SPDX-License-Identifier: MIT
#include "picker.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QScreen>
#include <QStyleHints>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const QStringList symbolMarks = {"↺", "♥", "©", "…", "€", "∑", "→", "◇", "①", "☺"};
const QStringList toneColors = {"#ffd166", "#f8dfc6", "#eac39b", "#c89460", "#966747", "#614337"};
QPixmap categoryIcon(int category, const QColor &color) {
    QPixmap image(36, 36); image.setDevicePixelRatio(2); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (category) {
    case 0:
        p.drawEllipse(QRectF(2.5, 2.5, 13, 13));
        p.drawLine(QPointF(9, 5), QPointF(9, 9)); p.drawLine(QPointF(9, 9), QPointF(12, 10.5));
        break;
    case 1: {
        QPainterPath path; path.moveTo(9, 15);
        path.cubicTo(-3, 7, 5, -1, 9, 5); path.cubicTo(13, -1, 21, 7, 9, 15);
        p.drawPath(path); break;
    }
    case 2:
        for (int x : {3, 10}) for (int y : {3, 10}) p.drawRoundedRect(QRectF(x, y, 5, 5), 1, 1);
        break;
    case 3:
        p.drawEllipse(QRectF(2.5, 2.5, 13, 13));
        p.drawPoint(QPointF(6.5, 7)); p.drawPoint(QPointF(11.5, 7));
        p.drawArc(QRectF(5.5, 7.5, 7, 5), 200*16, 140*16);
        break;
    case 4:
        p.drawEllipse(QRectF(6, 2, 6, 6));
        p.drawArc(QRectF(3, 9, 12, 12), 0, 180*16); p.drawLine(3, 15, 15, 15);
        break;
    case 5: {
        QPainterPath leaf; leaf.moveTo(4, 14);
        leaf.cubicTo(0, 5, 9, 2, 15, 3); leaf.cubicTo(16, 10, 12, 17, 4, 14);
        p.drawPath(leaf); p.drawLine(3, 16, 12, 6); break;
    }
    case 6:
        p.drawRoundedRect(QRectF(3, 6, 10, 8), 2, 2);
        p.drawArc(QRectF(11, 7, 5, 5), -90*16, 180*16);
        p.drawLine(3, 16, 14, 16); p.drawLine(6, 2, 6, 4); p.drawLine(10, 2, 10, 4);
        break;
    case 7: {
        QPainterPath plane; plane.moveTo(2, 7); plane.lineTo(16, 2); plane.lineTo(11, 16);
        plane.lineTo(8, 10); plane.closeSubpath(); p.drawPath(plane); p.drawLine(8, 10, 16, 2); break;
    }
    case 8:
        p.drawEllipse(QRectF(2.5, 2.5, 13, 13));
        p.drawLine(4, 4, 14, 14); p.drawLine(4, 14, 14, 4);
        p.drawArc(QRectF(-1, 5, 10, 14), 0, 100*16); p.drawArc(QRectF(9, -1, 10, 14), 180*16, 100*16);
        break;
    case 9:
        p.drawArc(QRectF(4, 2, 10, 11), -30*16, 240*16);
        p.drawLine(5, 10, 6, 13); p.drawLine(13, 10, 12, 13);
        p.drawLine(6, 13, 12, 13); p.drawLine(7, 16, 11, 16);
        break;
    case 10:
        p.drawLine(7, 3, 5, 15); p.drawLine(12, 3, 10, 15);
        p.drawLine(3, 7, 15, 7); p.drawLine(2, 11, 14, 11);
        break;
    case 11: {
        p.drawLine(4, 2, 4, 16); QPainterPath flag; flag.moveTo(4, 3);
        flag.cubicTo(8, 0, 10, 6, 15, 3); flag.lineTo(15, 10);
        flag.cubicTo(10, 13, 8, 7, 4, 10); p.drawPath(flag); break;
    }
    }
    return image;
}
QString folded(QString text) { return text.normalized(QString::NormalizationForm_KC).toCaseFolded(); }
QStringList splitWords(const QString &text) {
    return folded(text).split(QRegularExpression("[\\s\\p{P}]+"), Qt::SkipEmptyParts);
}
void loadEntries(const QString &path, bool symbols, QList<Emoji> &entries) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).object()["entries"].toArray()) {
        const auto o = value.toObject();
        Emoji e{o["text"].toString(), o["name"].toString(), o["english"].toString(), {}, o["category"].toInt(), {}, {}, {}, {}, {}};
        e.symbol = symbols;
        if (symbols) {
            const auto code = o["code"].toString();
            e.search = folded(e.text+" "+e.name+" "+e.english+" "+o["keywords"].toString()+" "+o["pinyin"].toString()+" "+code);
            for (const auto &alias : o["aliases"].toArray())
                e.words.append(folded(alias.toString()).split(QRegularExpression("[\\s\\p{P}]+"), Qt::SkipEmptyParts));
            const auto hex = code.mid(2);
            if (!hex.isEmpty()) e.words.append(hex);
        } else {
            e.search = folded(e.text+" "+o["keywords"].toString()+" "+o["pinyin"].toString());
            for (const auto &tone : o["tones"].toArray()) e.tones.append(tone.toString());
            for (const auto &alias : o["aliases"].toArray())
                e.words.append(folded(alias.toString()).split(QRegularExpression("[\\s\\p{P}]+"), Qt::SkipEmptyParts));
            QStringList englishAliases, chineseAliases, pinyinAliases;
            for (const auto &alias : o["english_aliases"].toArray()) englishAliases << alias.toString();
            for (const auto &alias : o["chinese_aliases"].toArray()) chineseAliases << alias.toString();
            for (const auto &alias : o["pinyin_aliases"].toArray()) pinyinAliases << alias.toString();
            if (englishAliases.isEmpty() && chineseAliases.isEmpty()) {
                e.chineseText = e.search;
                e.englishWords = e.pinyinWords = e.words;
            } else {
                e.chineseText = folded(e.name + " " + chineseAliases.join(' '));
                e.englishWords = splitWords(e.english + " " + englishAliases.join(' '));
                e.pinyinWords = splitWords(o["pinyin"].toString() + " " + pinyinAliases.join(' '));
            }
        }
        entries.append(e);
    }
}
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const {
    auto *e = entry(index.row());
    if (!e) return {};
    if (role == Qt::DisplayRole) return glyph(index.row());
    if (role == Qt::ToolTipRole) {
        const auto primary = displayName(*e), alternate = alternateName(*e);
        return alternate.isEmpty() ? primary : primary+"\n"+alternate;
    }
    if (role == Qt::AccessibleTextRole) return displayName(*e);
    return {};
}
QString EmojiModel::glyph(int row) const {
    auto *e = entry(row);
    return e ? e->tones.value(tone, e->text) : QString{};
}
const Emoji *EmojiModel::entry(int row) const {
    return row >= 0 && row < visible.size() ? &entries[visible[row]] : nullptr;
}
bool EmojiModel::favorite(int row) const {
    auto *e = entry(row); return e && favorites.contains(e->text);
}
QString EmojiModel::displayName(const Emoji &e) const {
    if (!englishUi) return e.name;
    if (e.english.isEmpty() || e.english.startsWith("U+")) return e.name;
    return e.english;
}
QString EmojiModel::alternateName(const Emoji &e) const {
    const auto primary = displayName(e);
    const auto alternate = englishUi ? e.name : e.english;
    return alternate == primary ? QString{} : alternate;
}
void EmojiModel::refresh() {
    beginResetModel(); visible.clear();
    const auto terms = folded(query.trimmed()).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    // Symbols are searched across every symbol tab, emoji stay in the current tab.
    const bool searchAllSymbols = symbols && !terms.empty();
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        if (e.symbol != symbols) continue;
        if (!searchAllSymbols) {
            if (category == -2 && !recents.contains(e.text)) continue;
            if (category == -3 && !favorites.contains(e.text)) continue;
            if (category >= 0 && category != e.category) continue;
        }
        bool matches = true;
        for (const auto &raw : terms) {
            // Accept identifiers copied straight from a Unicode table.
            auto term = raw;
            if (term.startsWith("u+")) term = term.mid(2);
            else if (term.startsWith("0x")) term = term.mid(2);
            if (term.isEmpty()) continue;
            const bool ascii = std::all_of(term.begin(), term.end(), [](QChar c) { return c.unicode() < 128; });
            const auto hasPrefix = [&](const QStringList &words) {
                return std::any_of(words.begin(), words.end(), [&](const QString &word) { return word.startsWith(term); });
            };
            const bool found = e.symbol ? (ascii ? hasPrefix(e.words) : e.search.contains(term))
                : ascii ? (hasPrefix(e.englishWords) || (searchPinyin && hasPrefix(e.pinyinWords)))
                        : (searchChinese && e.chineseText.contains(term));
            if (!found) { matches = false; break; }
        }
        if (matches) visible.append(i);
    }
    if (category == -2 || category == -3) {
        const auto &order = category == -2 ? recents : favorites;
        std::stable_sort(visible.begin(), visible.end(), [&](int a, int b) { return order.indexOf(entries[a].text) < order.indexOf(entries[b].text); });
    } else if (!terms.empty()) {
        const auto q = folded(query.trimmed());
        auto rank = [&](int i) {
            const auto &e = entries[i];
            if (folded(e.name) == q || folded(e.english) == q || e.text == q) return 0;
            if (folded(e.name).startsWith(q) || folded(e.english).startsWith(q)) return 1;
            return 2;
        };
        std::stable_sort(visible.begin(), visible.end(), [&](int a, int b) { return rank(a) < rank(b); });
    }
    endResetModel();
}

void EmojiDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const {
    p->save(); p->setRenderHint(QPainter::Antialiasing);
    const QRectF tile = opt.rect.adjusted(3, 3, -3, -3);
    if (opt.state & QStyle::State_Selected) {
        p->setBrush(colors.selected); p->setPen(QPen(colors.accent, 1.0)); p->drawRoundedRect(tile, 8, 8);
    } else if (opt.state & QStyle::State_MouseOver) {
        p->setBrush(colors.hover); p->setPen(Qt::NoPen); p->drawRoundedRect(tile, 8, 8);
    }
    QFont font = model_->symbols ? QApplication::font() : QFont("Noto Color Emoji");
    font.setPixelSize(model_->symbols ? 21 : 27);
    p->setFont(font); p->setPen(colors.text);
    p->drawText(opt.rect.adjusted(0, -1, 0, -1), Qt::AlignCenter, model_->glyph(index.row()));
    if (model_->favorite(index.row())) {
        p->setPen(Qt::NoPen); p->setBrush(colors.accent); p->drawEllipse(QPointF(tile.right()-5, tile.top()+5), 2.5, 2.5);
    }
    p->restore();
}

Picker::Picker(const QString &dataPath, bool preview, const QStringList &searchLanguages,
               const QString &theme, const QString &accent)
    : QWidget(nullptr), settings_("kmoji", "kmoji"), preview_(preview) {
    theme_ = theme == "light" || theme == "dark" ? theme : "system";
    accent_ = accent;
    model_.englishUi = QCoreApplication::translate("Picker", "Search emoji") == "Search emoji";
    if (!searchLanguages.isEmpty()) {
        model_.searchChinese = searchLanguages.contains("zh", Qt::CaseInsensitive)
            || searchLanguages.contains("chinese", Qt::CaseInsensitive);
        model_.searchPinyin = searchLanguages.contains("pinyin", Qt::CaseInsensitive);
    }
    // Keep this title stable: the KWin rule installed by scripts/install.py
    // matches it to give the panel its popup layer and focus behaviour.
    setWindowTitle("kMoji Emoji Picker");
    setObjectName("picker");
    if (!preview) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
    }
    setFixedSize(376, 420);
    const QFileInfo info(dataPath);
    loadEntries(dataPath, false, model_.entries);
    loadEntries(info.dir().filePath("symbols.json"), true, model_.entries);
    model_.favorites = settings_.value("favorites").toStringList();
    model_.recents = settings_.value("recents").toStringList();
    model_.tone = std::clamp(settings_.value("tone", 0).toInt(), 0, 5);
    buildUi(); applyTheme(); selectCategory(2); selectTone(model_.tone);
    hoverTimer_.setSingleShot(true); hoverTimer_.setInterval(550);
    connect(&hoverTimer_, &QTimer::timeout, this, &Picker::showHoverName);
    connect(grid_->verticalScrollBar(), &QScrollBar::valueChanged, this, &Picker::hideHoverName);
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (theme_ == "system") applyTheme(); });
    debounce_.setSingleShot(true); debounce_.setInterval(60);
    connect(&debounce_, &QTimer::timeout, this, &Picker::refresh);
    connect(search_, &QLineEdit::textChanged, this, [this] { debounce_.start(); });
    connect(&socket_, &QLocalSocket::connected, this, [this] { send("READY"); });
    connect(&socket_, &QLocalSocket::readyRead, this, [this] {
        input_ += socket_.readAll();
        while (input_.contains('\n')) {
            auto at = input_.indexOf('\n'); const auto line = input_.left(at); input_.remove(0, at+1); receive(line);
        }
    });
    connect(&socket_, &QLocalSocket::disconnected, qApp, &QApplication::quit);
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this](auto) { qWarning() << socket_.errorString(); qApp->quit(); });
}

void Picker::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 8); root->setSpacing(8);

    auto *searchRow = new QHBoxLayout; searchRow->setSpacing(6);
    search_ = new QLineEdit;
    search_->setPlaceholderText(tr("Search emoji")); search_->setClearButtonEnabled(true);
    search_->setFixedHeight(36); search_->setObjectName("search");
    search_->setAccessibleName(tr("Search emoji by English or pinyin"));
    search_->setToolTip(tr("Search by English, pinyin, or paste Chinese with Ctrl+V"));
    search_->installEventFilter(this);
    if (!preview_) search_->setFocusPolicy(Qt::NoFocus);
    searchRow->addWidget(search_, 1);
    auto *settings = new QToolButton;
    settings->setText("···"); settings->setObjectName("settingsButton");
    settings->setToolTip(tr("More options")); settings->setAccessibleName(tr("More options"));
    settings->setCheckable(true); settings->setFixedSize(28, 30);
    auto *close = new QToolButton;
    close->setText("×"); close->setFixedSize(24, 30);
    close->setToolTip(tr("Close · Esc")); close->setAccessibleName(tr("Close"));
    connect(close, &QToolButton::clicked, this, &Picker::cancel);
    searchRow->addWidget(settings); searchRow->addWidget(close);
    root->addLayout(searchRow);

    settingsPanel_ = new QFrame; settingsPanel_->setObjectName("options");
    auto *options = new QHBoxLayout(settingsPanel_);
    options->setContentsMargins(8, 6, 8, 6); options->setSpacing(8);
    continuous_ = new QCheckBox(tr("Continuous input"));
    continuous_->setChecked(settings_.value("continuous", false).toBool());
    continuous_->setToolTip(tr("Keep the panel open after inserting"));
    connect(continuous_, &QCheckBox::toggled, this, [this](bool on) { settings_.setValue("continuous", on); });
    options->addWidget(continuous_); options->addStretch();
    settingsPanel_->hide(); root->addWidget(settingsPanel_);
    connect(settings, &QToolButton::toggled, settingsPanel_, &QFrame::setVisible);

    auto *section = new QHBoxLayout; section->setSpacing(6);
    heading_ = new QLabel; heading_->setObjectName("heading");
    section->addWidget(heading_); section->addStretch();
    modeButton_ = new QToolButton;
    modeButton_->setObjectName("modeButton"); modeButton_->setText("Ω");
    modeButton_->setCheckable(true);
    modeButton_->setFixedSize(32, 26); modeButton_->setFocusPolicy(Qt::NoFocus);
    modeButton_->setToolTip(tr("Special symbols · Ctrl+Shift+S"));
    modeButton_->setAccessibleName(tr("Special symbols"));
    connect(modeButton_, &QToolButton::clicked, this, &Picker::toggleSymbols);
    const bool hasSymbols = std::any_of(model_.entries.cbegin(), model_.entries.cend(),
                                        [](const Emoji &entry) { return entry.symbol; });
    modeButton_->setEnabled(hasSymbols);
    if (!hasSymbols) modeButton_->setToolTip(tr("Symbol data not found"));
    skinButton_ = new QToolButton;
    skinButton_->setObjectName("skinButton"); skinButton_->setFixedSize(28, 26);
    skinButton_->setCheckable(true); skinButton_->setToolTip(tr("Choose skin tone"));
    skinButton_->setAccessibleName(tr("Choose skin tone"));
    section->addWidget(skinButton_); section->addWidget(modeButton_);
    root->addLayout(section);

    tonePanel_ = new QFrame; tonePanel_->setObjectName("options");
    auto *tones = new QHBoxLayout(tonePanel_); tones->setContentsMargins(8, 4, 8, 4);
    auto *toneLabel = new QLabel(tr("Skin tone")); toneLabel->setObjectName("muted");
    tones->addWidget(toneLabel); tones->addStretch();
    const QStringList toneNames = {tr("Default"), tr("Light"), tr("Slightly light"), tr("Medium"),
                                   tr("Slightly dark"), tr("Dark")};
    for (int i = 0; i < 6; ++i) {
        auto *tone = new QToolButton;
        tone->setFixedSize(24, 24); tone->setObjectName("tone"); tone->setCheckable(true);
        tone->setToolTip(toneNames[i]); tone->setAccessibleName(toneNames[i]);
        toneButtons_.append(tone); tones->addWidget(tone);
        connect(tone, &QToolButton::clicked, this, [this, i] {
            selectTone(i); skinButton_->setChecked(false);
        });
    }
    tonePanel_->hide(); root->addWidget(tonePanel_);
    connect(skinButton_, &QToolButton::toggled, tonePanel_, &QFrame::setVisible);

    grid_ = new QListView; grid_->setObjectName("grid"); grid_->setModel(&model_);
    delegate_ = new EmojiDelegate(&model_, grid_); grid_->setItemDelegate(delegate_);
    grid_->setViewMode(QListView::IconMode); grid_->setResizeMode(QListView::Adjust);
    grid_->setMovement(QListView::Static); grid_->setGridSize({42, 42});
    grid_->setUniformItemSizes(true); grid_->setSpacing(0);
    grid_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_->setMouseTracking(true); grid_->setFocusPolicy(Qt::NoFocus);
    grid_->viewport()->installEventFilter(this);
    connect(grid_, &QListView::clicked, this, [this](const QModelIndex &index) {
        choose(index.row(), shiftHeld_ || (QApplication::keyboardModifiers() & Qt::ShiftModifier));
    });
    empty_ = new QLabel(grid_->viewport()); empty_->setAlignment(Qt::AlignCenter);
    empty_->setWordWrap(true); empty_->setObjectName("empty");
    root->addWidget(grid_, 1);

    collectionPanel_ = new QFrame; collectionPanel_->setObjectName("collection");
    auto *collection = new QHBoxLayout(collectionPanel_);
    collection->setContentsMargins(7, 4, 5, 4); collection->setSpacing(5);
    collectionLabel_ = new QLabel; collectionLabel_->setObjectName("collected");
    collection->addWidget(collectionLabel_, 1);
    auto *clear = new QToolButton;
    clear->setText("×"); clear->setObjectName("clearCollection"); clear->setToolTip(tr("Clear selection"));
    connect(clear, &QToolButton::clicked, this, [this] { collected_.clear(); updateCollection(); });
    collection->addWidget(clear);
    insert_ = new QPushButton; insert_->setObjectName("insert"); insert_->setFixedHeight(27);
    connect(insert_, &QPushButton::clicked, this, &Picker::commit); collection->addWidget(insert_);
    collectionPanel_->hide(); root->addWidget(collectionPanel_);

    auto *categories = new QFrame; categories->setObjectName("categories");
    auto *nav = new QHBoxLayout(categories);
    nav->setContentsMargins(0, 5, 0, 0); nav->setSpacing(1);
    nav->addStretch(1);
    const auto categoryLabels = categoryNames();
    for (int i = 0; i < categoryLabels.size(); ++i) {
        auto *button = new QPushButton;
        button->setObjectName("category"); button->setCheckable(true);
        button->setFixedSize(28, 30); button->setIconSize({18, 18});
        button->setToolTip(categoryLabels[i]); button->setAccessibleName(categoryLabels[i]);
        button->setFocusPolicy(Qt::NoFocus);
        categoryButtons_.append(button); nav->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] { selectCategory(i); });
    }
    const auto symbolLabels = symbolCategoryNames();
    for (int i = 0; i < symbolLabels.size(); ++i) {
        auto *button = new QPushButton(symbolMarks[i]);
        button->setObjectName("category"); button->setCheckable(true);
        button->setProperty("symbolNav", true);
        button->setFixedSize(34, 30);
        button->setToolTip(symbolLabels[i]); button->setAccessibleName(symbolLabels[i]);
        button->setFocusPolicy(Qt::NoFocus); button->hide();
        symbolButtons_.append(button); nav->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] { selectCategory(i); });
    }
    nav->addStretch(1);
    root->addWidget(categories);
    // A child overlay also works while the Wayland window remains unfocused.
    hoverName_ = new QLabel(this); hoverName_->setObjectName("emojiName");
    hoverName_->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverName_->setTextFormat(Qt::PlainText); hoverName_->hide();
    updateCollection();
}

void Picker::applyTheme() {
    const bool dark = theme_ == "dark" || (theme_ == "system" && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    colors_ = dark
        ? Colors{"#26262b", "#26262b", "#303036", "#3c3c43", "#eeeeef", "#a3a3af", "#b6aaff", "#423b58", "#34343c"}
        : Colors{"#fafafa", "#fafafa", "#f0f0f2", "#e1e1e6", "#303037", "#7c7c88", "#7763d7", "#eee9fa", "#ececf0"};
    QString accentValue = accent_;
    if (accentValue.size() == 9 && accentValue.startsWith('#')) accentValue.truncate(7);
    QColor accent(accentValue);
    if (accent.isValid()) {
        auto mix = [](const QColor &a, const QColor &b, qreal t) {
            return QColor::fromRgbF(a.redF()*t + b.redF()*(1-t),
                                    a.greenF()*t + b.greenF()*(1-t),
                                    a.blueF()*t + b.blueF()*(1-t));
        };
        colors_.accent = accent;
        colors_.selected = mix(accent, colors_.background, dark ? 0.28 : 0.13);
    }
    const auto c = colors_;
    setStyleSheet(QString(R"(
      QWidget#picker { background:%1; color:%5; border:1px solid %4; border-radius:12px; }
      QWidget { font-family:"Noto Sans","sans-serif"; font-size:12px; color:%5; }
      QLabel#heading, QLabel#muted, QLabel#empty { color:%6; }
      QLabel#heading { font-size:11px; }
      QLabel#collected { font-size:19px; }
      QLineEdit#search { background:%3; color:%5; placeholder-text-color:%6; border:1px solid transparent; border-radius:7px; padding:0 10px; selection-background-color:%7; }
      QLineEdit#search:focus { border-color:%4; }
      QListView#grid { background:%2; border:0; outline:0; }
      QListView#grid::item { border:0; }
      QFrame#categories { border-top:1px solid %4; }
      QPushButton#category { background:transparent; border:0; border-bottom:2px solid transparent; border-radius:0; }
      QPushButton#category:hover { background:%9; border-radius:5px; }
      QPushButton#category:checked { border-bottom-color:%7; color:%7; }
      QPushButton#category[symbolNav="true"] { font-size:17px; padding-bottom:1px; }
      QToolButton { background:transparent; border:0; border-radius:5px; padding:3px; color:%6; }
      QToolButton:hover { background:%9; color:%5; }
      QToolButton:checked { background:%8; color:%7; }
      QToolButton#settingsButton { font-size:19px; font-weight:600; }
      QToolButton#skinButton { font-family:"Noto Color Emoji"; font-size:17px; padding:0; }
      QToolButton#modeButton { font-family:"Noto Sans","sans-serif"; font-size:17px; font-weight:600; padding:0; }
      QFrame#options, QFrame#collection { background:%3; border:0; border-radius:7px; }
      QCheckBox { spacing:6px; font-size:11px; }
      QCheckBox::indicator { width:12px; height:12px; border:1px solid %6; border-radius:3px; background:%1; }
      QCheckBox::indicator:checked { background:%7; border-color:%7; image:none; }
      QPushButton#insert { background:%7; color:white; border:0; border-radius:5px; padding:0 9px; font-size:11px; }
      QScrollBar:vertical { background:transparent; width:5px; margin:2px 0; }
      QScrollBar::handle:vertical { background:%4; border-radius:2px; min-height:24px; }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
      QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
      QLabel#emojiName { background:%3; color:%5; border:1px solid %4; border-radius:6px; padding:7px 9px; font-size:11px; }
      QToolTip { background:%3; color:%5; border:1px solid %4; padding:6px; }
    )").arg(c.background.name(), c.surface.name(), c.sidebar.name(), c.border.name(), c.text.name(), c.muted.name(), c.accent.name(), c.selected.name(), c.hover.name()));
    delegate_->colors = colors_;
    auto palette = search_->palette();
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::PlaceholderText, c.muted);
    search_->setPalette(palette);
    for (int i = 0; i < toneButtons_.size(); ++i) {
        toneButtons_[i]->setStyleSheet(QString("QToolButton { background:%1; border:4px solid %2; border-radius:12px; padding:0; } QToolButton:checked { border-color:%3; }").arg(toneColors[i], colors_.sidebar.name(), colors_.accent.name()));
    }
    for (int i = 0; i < categoryButtons_.size(); ++i) {
        QIcon icon;
        icon.addPixmap(categoryIcon(i, c.muted), QIcon::Normal, QIcon::Off);
        icon.addPixmap(categoryIcon(i, c.accent), QIcon::Normal, QIcon::On);
        icon.addPixmap(categoryIcon(i, c.text), QIcon::Active, QIcon::Off);
        icon.addPixmap(categoryIcon(i, c.accent), QIcon::Active, QIcon::On);
        categoryButtons_[i]->setIcon(icon);
    }
    grid_->viewport()->update();
}

QStringList Picker::categoryNames() const {
    return {tr("Recent"), tr("Favorites"), tr("All emoji"), tr("Smileys & Emotion"),
            tr("People & Body"), tr("Animals & Nature"), tr("Food & Drink"), tr("Travel & Places"),
            tr("Activities"), tr("Objects"), tr("Symbols"), tr("Flags")};
}
QStringList Picker::symbolCategoryNames() const {
    return {tr("Recent"), tr("Favorites"), tr("Common"), tr("Punctuation"), tr("Currency"),
            tr("Math"), tr("Arrows"), tr("Shapes"), tr("Letters & Numbers"), tr("Other")};
}
void Picker::selectCategory(int index) {
    if (symbols_) {
        const auto names = symbolCategoryNames();
        navIndex_ = (index+names.size()) % names.size();
        for (int i = 0; i < symbolButtons_.size(); ++i) symbolButtons_[i]->setChecked(i == navIndex_);
        model_.category = navIndex_ == 0 ? -2 : navIndex_ == 1 ? -3 : navIndex_-2;
    } else {
        const auto names = categoryNames();
        navIndex_ = (index+names.size()) % names.size();
        for (int i = 0; i < categoryButtons_.size(); ++i) categoryButtons_[i]->setChecked(i == navIndex_);
        model_.category = navIndex_ == 0 ? -2 : navIndex_ == 1 ? -3 : navIndex_ == 2 ? -1 : navIndex_-3;
    }
    refresh();
}
void Picker::setSymbolMode(bool symbols) {
    symbols_ = symbols; model_.symbols = symbols;
    modeButton_->setChecked(symbols);
    modeButton_->setToolTip(symbols ? tr("Back to emoji · Ctrl+Shift+S") : tr("Special symbols · Ctrl+Shift+S"));
    modeButton_->setAccessibleName(symbols ? tr("Back to emoji") : tr("Special symbols"));
    skinButton_->setVisible(!symbols);
    if (symbols) { skinButton_->setChecked(false); tonePanel_->hide(); }
    for (auto *button : categoryButtons_) button->setVisible(!symbols);
    for (auto *button : symbolButtons_) button->setVisible(symbols);
    search_->clear();
    search_->setPlaceholderText(symbols ? tr("Search symbols") : tr("Search emoji"));
    search_->setAccessibleName(symbols ? tr("Search symbols by Chinese, English, or code point")
                                       : tr("Search emoji by English or pinyin"));
    search_->setToolTip(symbols ? tr("Search by Chinese, English name, or code point, e.g. 2192")
                                : tr("Search by English, pinyin, or paste Chinese with Ctrl+V"));
    selectCategory(2);
}
void Picker::toggleSymbols() { setSymbolMode(!symbols_); }
void Picker::selectTone(int tone) {
    model_.tone = tone; settings_.setValue("tone", tone);
    for (int i = 0; i < toneButtons_.size(); ++i) toneButtons_[i]->setChecked(i == tone);
    const QStringList hands = {"✋", "✋🏻", "✋🏼", "✋🏽", "✋🏾", "✋🏿"};
    skinButton_->setText(hands[tone]);
    grid_->viewport()->update();
}
void Picker::refresh() {
    hideHoverName();
    debounce_.stop(); model_.query = search_->text(); model_.refresh();
    const auto names = symbols_ ? symbolCategoryNames() : categoryNames();
    heading_->setText(search_->text().isEmpty() ? names[navIndex_] : tr("Search results"));
    grid_->setCurrentIndex(model_.index(0)); grid_->scrollToTop();
    empty_->setGeometry(grid_->viewport()->rect().adjusted(25, 0, -25, 0));
    if (symbols_) empty_->setText(!search_->text().isEmpty()
        ? tr("No matching symbols\nTry “arrows”, “currency”, or “math”")
        : navIndex_ == 0 ? tr("No recent symbols yet\nPick a symbol to get started")
        : navIndex_ == 1 ? tr("Favorite symbols appear here\nRight-click any symbol to add it")
                         : tr("No symbols available"));
    else empty_->setText(!search_->text().isEmpty()
        ? tr("No matching emoji\nTry another keyword, or browse All emoji")
        : navIndex_ == 1 ? tr("Favorite emoji appear here\nRight-click any emoji to add it")
                         : tr("No recent emoji yet\nPick one and start expressing"));
    empty_->setVisible(model_.visible.isEmpty());
}
void Picker::toggleFavorite(int row) {
    const auto *e = model_.entry(row); if (!e) return;
    if (model_.favorites.contains(e->text)) model_.favorites.removeAll(e->text);
    else model_.favorites.append(e->text);
    saveHistory();
    if (navIndex_ == 1) refresh(); else grid_->viewport()->update();
}
void Picker::saveHistory() {
    settings_.setValue("favorites", model_.favorites); settings_.setValue("recents", model_.recents); settings_.sync();
}
void Picker::choose(int row, bool collect) {
    const auto *e = model_.entry(row); if (!e) return;
    collected_.append({e->text, model_.glyph(row)});
    if (collect) updateCollection(); else commit();
}
void Picker::commit() {
    if (collected_.isEmpty()) {
        const int row = grid_->currentIndex().row();
        const auto *e = model_.entry(row); if (!e) return;
        collected_.append({e->text, model_.glyph(row)});
    }
    QString text;
    for (const auto &pair : collected_) {
        text += pair.second; model_.recents.removeAll(pair.first); model_.recents.prepend(pair.first);
    }
    while (model_.recents.size() > 80) model_.recents.removeLast();
    saveHistory();
    if (preview_) heading_->setText(tr("Chosen: %1").arg(text));
    else if (session_) send("COMMIT\t"+token_+"\t"+text.toUtf8().toHex()+"\t"+(continuous_->isChecked() ? "keep" : "close"));
    collected_.clear(); updateCollection();
    if (!continuous_->isChecked() && !preview_) { session_ = false; hide(); }
}
void Picker::updateCollection() {
    QString preview;
    for (const auto &e : collected_) preview += e.second;
    collectionLabel_->setText(collectionLabel_->fontMetrics().elidedText(preview, Qt::ElideRight, 230));
    const int count = collected_.size();
    insert_->setText(count == 1 ? tr("Insert 1 item") : tr("Insert %1 items").arg(count));
    collectionPanel_->setVisible(!collected_.isEmpty());
}
void Picker::moveSelection(int delta) {
    if (model_.visible.isEmpty()) return;
    const int next = std::clamp(grid_->currentIndex().row()+delta, 0, model_.rowCount()-1);
    const auto index = model_.index(next); grid_->setCurrentIndex(index); grid_->scrollTo(index);
}
void Picker::handleKey(int sym, int states, const QString &text) {
    hideHoverName();
    const bool ctrl = states & 4, shift = states & 1, alt = states & 8, super = states & 64;
    const int columns = std::max(1, grid_->viewport()->width()/grid_->gridSize().width());
    if (sym == 0xff1b) { cancel(); return; }
    if (ctrl && shift && (sym == 's' || sym == 'S')) { toggleSymbols(); return; }
    if (alt && (sym == 0xff51 || sym == 0xff53)) { selectCategory(navIndex_+(sym == 0xff51 ? -1 : 1)); return; }
    if (sym == 0xff09 || sym == 0xfe20) { selectCategory(navIndex_+(shift ? -1 : 1)); return; }
    if (sym == 0xff0d || sym == 0xff8d) {
        debounce_.stop(); if (model_.query != search_->text()) refresh();
        if (shift) choose(grid_->currentIndex().row(), true); else commit(); return;
    }
    if (ctrl && (sym == 'd' || sym == 'D')) { toggleFavorite(grid_->currentIndex().row()); return; }
    if (sym == 0xff51) { moveSelection(-1); return; }
    if (sym == 0xff53) { moveSelection(1); return; }
    if (sym == 0xff52) { moveSelection(-columns); return; }
    if (sym == 0xff54) { moveSelection(columns); return; }
    if (sym == 0xff55) { moveSelection(-columns*5); return; }
    if (sym == 0xff56) { moveSelection(columns*5); return; }
    if (sym == 0xff50) { moveSelection(-model_.rowCount()); return; }
    if (sym == 0xff57) { moveSelection(model_.rowCount()); return; }
    if (ctrl && (sym == 'v' || sym == 'V')) { search_->insert(QApplication::clipboard()->text()); return; }
    if (super || alt) return;
    int key = 0;
    if (sym == 0xff08) key = Qt::Key_Backspace;
    else if (sym == 0xffff) key = Qt::Key_Delete;
    else if (sym < 128) key = QChar(sym).toUpper().unicode();
    if (!key && text.isEmpty()) return;
    Qt::KeyboardModifiers modifiers;
    if (ctrl) modifiers |= Qt::ControlModifier;
    if (shift) modifiers |= Qt::ShiftModifier;
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(search_, &event);
}
void Picker::hideHoverName() {
    hoverTimer_.stop(); hoveredRow_ = -1;
    if (hoverName_) hoverName_->hide();
}
void Picker::trackHover(const QPoint &position) {
    const auto index = grid_->indexAt(position);
    const int row = index.isValid() ? index.row() : -1;
    if (row == hoveredRow_) return;
    hideHoverName(); hoveredRow_ = row;
    if (row >= 0) hoverTimer_.start();
}
void Picker::showHoverName() {
    const auto *e = model_.entry(hoveredRow_);
    if (!e || !isVisible()) return;
    const auto metrics = hoverName_->fontMetrics();
    const auto primary = model_.displayName(*e), alternate = model_.alternateName(*e);
    QString text = metrics.elidedText(primary, Qt::ElideRight, 270);
    if (!alternate.isEmpty()) text += "\n"+metrics.elidedText(alternate, Qt::ElideRight, 270);
    hoverName_->setText(text);
    hoverName_->adjustSize();
    const QRect cell = grid_->visualRect(model_.index(hoveredRow_));
    const QPoint top = grid_->viewport()->mapTo(this, cell.topLeft());
    const int x = std::clamp(top.x()+(cell.width()-hoverName_->width())/2, 8, width()-hoverName_->width()-8);
    int y = top.y()+cell.height()+5;
    const int gridBottom = grid_->viewport()->mapTo(this, QPoint(0, grid_->viewport()->height())).y();
    if (y+hoverName_->height() > gridBottom) y = top.y()-hoverName_->height()-5;
    hoverName_->move(x, std::clamp(y, 8, height()-hoverName_->height()-8));
    hoverName_->raise(); hoverName_->show();
}
bool Picker::eventFilter(QObject *object, QEvent *event) {
    if (grid_ && object == grid_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove:
            trackHover(static_cast<QMouseEvent *>(event)->position().toPoint());
            break;
        case QEvent::MouseButtonPress: {
            hideHoverName();
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::RightButton) { toggleFavorite(grid_->indexAt(mouse->position().toPoint()).row()); return true; }
            break;
        }
        case QEvent::Leave:
        case QEvent::Wheel:
        case QEvent::Resize:
            hideHoverName();
            break;
        case QEvent::ToolTip:
            // Suppress Qt's separate tooltip window; the child label owns it.
            return true;
        default: break;
        }
    }
    if (object == search_ && event->type() == QEvent::KeyPress && preview_) {
        auto *key = static_cast<QKeyEvent *>(event);
        int sym = 0;
        switch (key->key()) {
            case Qt::Key_Escape: sym=0xff1b; break;
            case Qt::Key_Return: sym=0xff0d; break;
            case Qt::Key_Left: sym=0xff51; break;
            case Qt::Key_Right: sym=0xff53; break;
            case Qt::Key_Up: sym=0xff52; break;
            case Qt::Key_Down: sym=0xff54; break;
            case Qt::Key_Tab: sym=0xff09; break;
            case Qt::Key_S:
                if (key->modifiers() & Qt::ControlModifier) sym = 'S';
                break;
            default: break;
        }
        if (sym) {
            handleKey(sym, (key->modifiers() & Qt::ShiftModifier ? 1 : 0) | (key->modifiers() & Qt::ControlModifier ? 4 : 0), key->text());
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}
void Picker::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) cancel();
    else QWidget::keyPressEvent(event);
}
void Picker::cancel() {
    if (session_) send("CANCEL\t"+token_);
    session_ = false; hide(); if (preview_) qApp->quit();
}
void Picker::closeEvent(QCloseEvent *event) { event->ignore(); cancel(); }
void Picker::hideEvent(QHideEvent *event) { hideHoverName(); QWidget::hideEvent(event); }
void Picker::connectToAddon(const QString &path) { socket_.connectToServer(path); }
void Picker::send(const QByteArray &line) { socket_.write(line+'\n'); socket_.flush(); }
void Picker::setPreviewQuery(const QString &query) { search_->setText(query); refresh(); if (preview_) search_->setFocus(); }
void Picker::openSession(const QByteArray &token) {
    token_ = token; session_ = true; shiftHeld_ = false; collected_.clear();
    if (symbols_) setSymbolMode(false);
    search_->clear(); selectCategory(2); updateCollection();
    findChild<QToolButton *>("settingsButton")->setChecked(false);
    skinButton_->setChecked(false);
    // KWin positions Wayland windows. X11 previews can be centered directly.
    if (auto *screen = QGuiApplication::primaryScreen()) move(screen->availableGeometry().center()-rect().center());
    show();
}
void Picker::receive(const QByteArray &line) {
    const auto parts = line.split('\t');
    if (parts[0] == "QUIT") { qApp->quit(); return; }
    if (parts[0] == "SHOW" && parts.size() >= 2) { openSession(parts[1]); return; }
    if (parts.size() < 2 || parts[1] != token_) return;
    if (parts[0] == "HIDE") { session_ = false; shiftHeld_ = false; hide(); return; }
    if (parts[0] == "KEY" && parts.size() == 6) {
        const int sym = parts[2].toInt(), states = parts[3].toInt();
        const bool release = parts[4] != "0";
        // A passive Wayland window does not receive wl_keyboard.modifiers.
        // Keep Shift from the input-method stream for mouse multi-selection.
        shiftHeld_ = states & 1;
        if (sym == 0xffe1 || sym == 0xffe2) shiftHeld_ = !release;
        if (!release) handleKey(sym, states, QString::fromUtf8(QByteArray::fromHex(parts[5])));
    }
}
