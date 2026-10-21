// SPDX-License-Identifier: MIT
#include "picker.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
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
const QStringList categoryNames = {"全部表情", "最近使用", "我的收藏", "笑脸与情感", "人物与手势", "动物与自然", "食物与饮品", "旅行与地点", "活动与运动", "物品与工具", "符号", "旗帜"};
const QStringList navNames = {"最近使用", "我的收藏", "全部表情", "笑脸与情感", "人物与手势", "动物与自然", "食物与饮品", "旅行与地点", "活动与运动", "物品与工具", "符号", "旗帜"};
const QStringList navSymbols = {"◷", "♡", "⊞", "☺", "☝", "❀", "♨", "✈", "⚑", "♧", "⌘", "⚐"};
const QStringList toneColors = {"#ffd166", "#f8dfc6", "#eac39b", "#c89460", "#966747", "#614337"};
QString folded(QString text) { return text.normalized(QString::NormalizationForm_KC).toCaseFolded(); }
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const {
    auto *e = entry(index.row());
    if (!e) return {};
    if (role == Qt::DisplayRole) return glyph(index.row());
    if (role == Qt::ToolTipRole) return e->name+"\n"+e->english+"\n右键收藏 · Shift 点选组合";
    if (role == Qt::AccessibleTextRole) return e->name;
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
void EmojiModel::refresh() {
    beginResetModel(); visible.clear();
    const auto terms = folded(query.trimmed()).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        if (category == -2 && !recents.contains(e.text)) continue;
        if (category == -3 && !favorites.contains(e.text)) continue;
        if (category >= 0 && category != e.category) continue;
        bool matches = true;
        for (const auto &term : terms) {
            const bool ascii = std::all_of(term.begin(), term.end(), [](QChar c) { return c.unicode() < 128; });
            const bool found = ascii ? std::any_of(e.words.begin(), e.words.end(), [&](const QString &word) { return word.startsWith(term); }) : e.search.contains(term);
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
        p->setBrush(colors.selected); p->setPen(QPen(colors.accent, 1.3)); p->drawRoundedRect(tile, 11, 11);
    } else if (opt.state & QStyle::State_MouseOver) {
        p->setBrush(colors.hover); p->setPen(Qt::NoPen); p->drawRoundedRect(tile, 11, 11);
    }
    QFont font("Noto Color Emoji"); font.setPixelSize(29);
    p->setFont(font); p->setPen(colors.text);
    p->drawText(opt.rect.adjusted(0, -1, 0, -1), Qt::AlignCenter, model_->glyph(index.row()));
    if (model_->favorite(index.row())) {
        p->setPen(Qt::NoPen); p->setBrush(colors.accent); p->drawEllipse(QPointF(tile.right()-5, tile.top()+5), 2.5, 2.5);
    }
    p->restore();
}

Picker::Picker(const QString &dataPath, bool preview) : QWidget(nullptr), settings_("kmoji", "kmoji"), preview_(preview) {
    setWindowTitle("kMoji Emoji Picker");
    setObjectName("picker");
    if (!preview) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
    }
    setFixedSize(652, 594);
    QFile file(dataPath);
    if (file.open(QIODevice::ReadOnly)) {
        for (const auto &value : QJsonDocument::fromJson(file.readAll()).object()["entries"].toArray()) {
            const auto o = value.toObject();
            Emoji e{o["text"].toString(), o["name"].toString(), o["english"].toString(), {}, o["category"].toInt(), {}, {}};
            e.search = folded(e.text+" "+o["keywords"].toString()+" "+o["pinyin"].toString());
            for (const auto &tone : o["tones"].toArray()) e.tones.append(tone.toString());
            for (const auto &alias : o["aliases"].toArray()) e.words.append(folded(alias.toString()).split(QRegularExpression("[\\s\\p{P}]+"), Qt::SkipEmptyParts));
            model_.entries.append(e);
        }
    }
    model_.favorites = settings_.value("favorites").toStringList();
    model_.recents = settings_.value("recents").toStringList();
    model_.tone = std::clamp(settings_.value("tone", 0).toInt(), 0, 5);
    theme_ = settings_.value("theme", "system").toString();
    buildUi(); applyTheme(); selectCategory(2); selectTone(model_.tone);
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
    auto *root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *top = new QFrame; top->setObjectName("top");
    auto *topLayout = new QHBoxLayout(top); topLayout->setContentsMargins(22, 17, 18, 17); topLayout->setSpacing(10);
    auto *logo = new QLabel("k"); logo->setObjectName("logo"); logo->setAlignment(Qt::AlignCenter); logo->setFixedSize(30, 30);
    topLayout->addWidget(logo);
    auto *brand = new QLabel("kMoji"); brand->setObjectName("brand"); topLayout->addWidget(brand);
    auto *tag = new QLabel("让表达多一点表情"); tag->setObjectName("muted"); topLayout->addWidget(tag);
    topLayout->addStretch();
    auto *theme = new QToolButton; theme->setObjectName("theme"); theme->setText("◐"); theme->setToolTip("切换外观：跟随系统 / 浅色 / 深色"); theme->setFixedSize(30, 30);
    connect(theme, &QToolButton::clicked, this, [this, theme] {
        theme_ = theme_ == "system" ? "light" : theme_ == "light" ? "dark" : "system";
        settings_.setValue("theme", theme_); applyTheme();
        theme->setToolTip("当前外观：" + (theme_ == "system" ? QString("跟随系统") : theme_ == "dark" ? QString("深色") : QString("浅色")));
    });
    topLayout->addWidget(theme);
    auto *close = new QToolButton; close->setText("×"); close->setToolTip("关闭 · Esc"); close->setFixedSize(30, 30);
    connect(close, &QToolButton::clicked, this, &Picker::cancel); topLayout->addWidget(close);
    root->addWidget(top);

    auto *body = new QHBoxLayout; body->setContentsMargins(0, 0, 0, 0); body->setSpacing(0);
    auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(146);
    auto *nav = new QVBoxLayout(sidebar); nav->setContentsMargins(10, 16, 10, 14); nav->setSpacing(4);
    for (int i = 0; i < navNames.size(); ++i) {
        if (i == 3) { auto *line = new QFrame; line->setObjectName("divider"); line->setFixedHeight(1); nav->addSpacing(7); nav->addWidget(line); nav->addSpacing(7); }
        auto *button = new QPushButton(navSymbols[i]+"   "+navNames[i]);
        button->setObjectName("category"); button->setCheckable(true); button->setFixedHeight(31); button->setFocusPolicy(Qt::NoFocus);
        categoryButtons_.append(button); nav->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] { selectCategory(i); });
    }
    nav->addStretch();
    auto *version = new QLabel("UNICODE 17.0"); version->setObjectName("version"); nav->addWidget(version);
    body->addWidget(sidebar);

    auto *main = new QFrame; main->setObjectName("main");
    auto *content = new QVBoxLayout(main); content->setContentsMargins(19, 17, 19, 10); content->setSpacing(12);
    search_ = new QLineEdit; search_->setPlaceholderText("搜索表情、英文或拼音…"); search_->setClearButtonEnabled(true);
    search_->setFixedHeight(42); search_->setObjectName("search"); search_->installEventFilter(this);
    if (!preview_) search_->setFocusPolicy(Qt::NoFocus);
    content->addWidget(search_);
    auto *section = new QHBoxLayout; section->setSpacing(8);
    heading_ = new QLabel; heading_->setObjectName("heading"); section->addWidget(heading_);
    count_ = new QLabel; count_->setObjectName("count"); section->addWidget(count_); section->addStretch();
    for (int i = 0; i < 6; ++i) {
        auto *tone = new QToolButton; tone->setFixedSize(19, 25); tone->setObjectName("tone"); tone->setCheckable(true);
        tone->setToolTip(i ? QString("肤色 %1").arg(i) : QString("默认肤色")); toneButtons_.append(tone); section->addWidget(tone);
        connect(tone, &QToolButton::clicked, this, [this, i] { selectTone(i); });
    }
    content->addLayout(section);
    grid_ = new QListView; grid_->setObjectName("grid"); grid_->setModel(&model_);
    delegate_ = new EmojiDelegate(&model_, grid_); grid_->setItemDelegate(delegate_);
    grid_->setViewMode(QListView::IconMode); grid_->setResizeMode(QListView::Adjust); grid_->setMovement(QListView::Static);
    grid_->setGridSize({54, 54}); grid_->setUniformItemSizes(true); grid_->setSpacing(0);
    grid_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); grid_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection); grid_->setMouseTracking(true); grid_->setFocusPolicy(Qt::NoFocus);
    grid_->viewport()->installEventFilter(this);
    connect(grid_, &QListView::clicked, this, [this](const QModelIndex &index) { choose(index.row(), shiftHeld_ || (QApplication::keyboardModifiers() & Qt::ShiftModifier)); });
    connect(grid_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateDetails(); });
    empty_ = new QLabel(grid_->viewport()); empty_->setAlignment(Qt::AlignCenter); empty_->setWordWrap(true); empty_->setObjectName("empty");
    content->addWidget(grid_, 1);
    auto *collection = new QFrame; collection->setObjectName("collection"); collection->setMinimumHeight(36);
    auto *collectionLayout = new QHBoxLayout(collection); collectionLayout->setContentsMargins(10, 4, 5, 4);
    collectionLabel_ = new QLabel("Shift 点选，将多个表情组合输入"); collectionLabel_->setObjectName("muted"); collectionLayout->addWidget(collectionLabel_, 1);
    auto *clear = new QToolButton; clear->setText("清空"); clear->setObjectName("clearCollection"); clear->setToolTip("清空已选表情");
    connect(clear, &QToolButton::clicked, this, [this] { collected_.clear(); updateCollection(); }); collectionLayout->addWidget(clear);
    insert_ = new QPushButton("输入"); insert_->setObjectName("insert"); insert_->setFixedHeight(28); insert_->setMinimumWidth(64);
    connect(insert_, &QPushButton::clicked, this, &Picker::commit); collectionLayout->addWidget(insert_);
    content->addWidget(collection);
    details_ = new QLabel(" "); details_->setObjectName("details"); details_->setFixedHeight(19); content->addWidget(details_);
    body->addWidget(main, 1); root->addLayout(body, 1);
    auto *footer = new QFrame; footer->setObjectName("footer");
    auto *bottom = new QHBoxLayout(footer); bottom->setContentsMargins(17, 10, 17, 10); bottom->setSpacing(10);
    continuous_ = new QCheckBox("连续输入"); continuous_->setChecked(settings_.value("continuous", false).toBool());
    continuous_->setToolTip("输入后保留面板，按 Esc 关闭");
    connect(continuous_, &QCheckBox::toggled, this, [this](bool on) { settings_.setValue("continuous", on); });
    bottom->addWidget(continuous_); bottom->addStretch();
    status_ = new QLabel(preview_ ? "界面预览" : "↵ 输入   ·   右键收藏   ·   Esc 关闭"); status_->setObjectName("muted"); bottom->addWidget(status_);
    root->addWidget(footer); updateCollection();
}

void Picker::applyTheme() {
    const bool dark = theme_ == "dark" || (theme_ == "system" && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    colors_ = dark
        ? Colors{"#1b1d27", "#222530", "#1c1f29", "#343746", "#eeeff8", "#959bb1", "#a69aff", "#37334e", "#2d303f"}
        : Colors{"#ffffff", "#ffffff", "#f6f7fb", "#e9eaf2", "#292b3b", "#8b90a4", "#7060dc", "#eeebfc", "#f2f3f9"};
    const auto c = colors_;
    auto sheet = QString(R"(
      QWidget#picker { background:%1; color:%5; border:1px solid %4; border-radius:14px; }
      QWidget { font-family:"Noto Sans","sans-serif"; font-size:12px; color:%5; }
      QFrame#top { background:%1; border-bottom:1px solid %4; border-top-left-radius:14px; border-top-right-radius:14px; }
      QLabel#logo { background:%7; color:white; border-radius:9px; font-size:24px; font-weight:700; }
      QLabel#brand { font-size:19px; font-weight:700; }
      QLabel#muted, QLabel#count, QLabel#version, QLabel#details, QLabel#empty { color:%6; }
      QLabel#version { font-size:9px; letter-spacing:1px; padding-left:9px; }
      QLabel#heading { font-size:13px; font-weight:600; }
      QLabel#count { font-size:11px; }
      QLabel#details { font-size:11px; }
      QFrame#sidebar { background:%3; border-right:1px solid %4; }
      QFrame#main { background:%2; }
      QFrame#divider { background:%4; border:0; }
      QPushButton#category { text-align:left; padding-left:11px; border:0; border-radius:8px; background:transparent; color:%6; }
      QPushButton#category:hover { background:%9; color:%5; }
      QPushButton#category:checked { background:%8; color:%7; font-weight:600; }
      QLineEdit#search { background:%3; border:1px solid %4; border-radius:10px; padding:0 12px; selection-background-color:%7; }
      QLineEdit#search:focus { border:1px solid %7; }
      QListView#grid { background:transparent; border:0; outline:0; }
      QListView#grid::item { border:0; }
      QScrollBar:vertical { background:transparent; width:6px; margin:2px 0; }
      QScrollBar::handle:vertical { background:%4; border-radius:3px; min-height:28px; }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
      QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
      QFrame#collection { background:%3; border:1px solid %4; border-radius:9px; }
      QPushButton#insert { background:%7; color:white; border:0; border-radius:7px; padding:0 12px; font-weight:600; }
      QPushButton#insert:disabled { background:%4; color:%6; }
      QToolButton { background:transparent; border:0; border-radius:6px; padding:3px; color:%6; }
      QToolButton:hover { background:%8; color:%7; }
      QToolButton#theme { font-size:19px; }
      QFrame#footer { background:%3; border-top:1px solid %4; border-bottom-left-radius:14px; border-bottom-right-radius:14px; }
      QCheckBox { spacing:6px; color:%6; font-size:11px; }
      QCheckBox::indicator { width:13px; height:13px; border:1px solid %6; border-radius:4px; background:%2; }
      QCheckBox::indicator:checked { background:%7; border-color:%7; image:none; }
      QToolTip { background:%3; color:%5; border:1px solid %4; padding:7px; }
    )").arg(c.background.name(), c.surface.name(), c.sidebar.name(), c.border.name(), c.text.name(), c.muted.name(), c.accent.name(), c.selected.name(), c.hover.name());
    setStyleSheet(sheet);
    delegate_->colors = colors_;
    for (int i = 0; i < toneButtons_.size(); ++i) {
        toneButtons_[i]->setStyleSheet(QString("QToolButton { background:%1; border:3px solid %2; border-radius:9px; padding:0; } QToolButton:checked { border-color:%3; }").arg(toneColors[i], colors_.surface.name(), colors_.accent.name()));
    }
    grid_->viewport()->update();
}

void Picker::selectCategory(int index) {
    navIndex_ = (index+navNames.size()) % navNames.size();
    for (int i = 0; i < categoryButtons_.size(); ++i) categoryButtons_[i]->setChecked(i == navIndex_);
    model_.category = navIndex_ == 0 ? -2 : navIndex_ == 1 ? -3 : navIndex_ == 2 ? -1 : navIndex_-3;
    refresh();
}
void Picker::selectTone(int tone) {
    model_.tone = tone; settings_.setValue("tone", tone);
    for (int i = 0; i < toneButtons_.size(); ++i) toneButtons_[i]->setChecked(i == tone);
    grid_->viewport()->update(); updateDetails();
}
void Picker::refresh() {
    debounce_.stop(); model_.query = search_->text(); model_.refresh();
    heading_->setText(search_->text().isEmpty() ? navNames[navIndex_] : "搜索结果");
    count_->setText(QString::number(model_.rowCount())+" 个");
    grid_->setCurrentIndex(model_.index(0)); grid_->scrollToTop();
    empty_->setGeometry(grid_->viewport()->rect().adjusted(25, 0, -25, 0));
    empty_->setText(!search_->text().isEmpty() ? "没有找到对应的表情\n试试其他关键词，或切换到全部表情" : navIndex_ == 1 ? "把喜欢的表情收在这里\n右键点击任意表情即可收藏" : "这里还没有最近使用的表情\n选一个表情，开始表达吧");
    empty_->setVisible(model_.visible.isEmpty());
    updateDetails();
}
void Picker::updateDetails() {
    const auto *e = model_.entry(grid_->currentIndex().row());
    if (!e) { details_->setText(" "); return; }
    const QString description = e->name+"  ·  "+e->english;
    details_->setToolTip(description);
    details_->setText(details_->fontMetrics().elidedText(description, Qt::ElideRight, 450));
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
    if (preview_) status_->setText("预览输入："+text);
    else if (session_) send("COMMIT\t"+token_+"\t"+text.toUtf8().toHex()+"\t"+(continuous_->isChecked() ? "keep" : "close"));
    collected_.clear(); updateCollection();
    if (!continuous_->isChecked() && !preview_) { session_ = false; hide(); }
}
void Picker::updateCollection() {
    QString preview;
    for (const auto &e : collected_) preview += e.second;
    collectionLabel_->setText(preview.isEmpty() ? QString("Shift 点选，将多个表情组合输入") : collectionLabel_->fontMetrics().elidedText(preview, Qt::ElideRight, 300));
    insert_->setText(collected_.isEmpty() ? "输入 ↵" : QString("输入 %1 个").arg(collected_.size()));
    if (auto *clear = findChild<QToolButton *>("clearCollection")) clear->setVisible(!collected_.isEmpty());
}
void Picker::moveSelection(int delta) {
    if (model_.visible.isEmpty()) return;
    const int next = std::clamp(grid_->currentIndex().row()+delta, 0, model_.rowCount()-1);
    const auto index = model_.index(next); grid_->setCurrentIndex(index); grid_->scrollTo(index);
}
void Picker::handleKey(int sym, int states, const QString &text) {
    const bool ctrl = states & 4, shift = states & 1, alt = states & 8, super = states & 64;
    const int columns = std::max(1, grid_->viewport()->width()/grid_->gridSize().width());
    if (sym == 0xff1b) { cancel(); return; }
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
bool Picker::eventFilter(QObject *object, QEvent *event) {
    if (grid_ && object == grid_->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::RightButton) { toggleFavorite(grid_->indexAt(mouse->position().toPoint()).row()); return true; }
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
void Picker::connectToAddon(const QString &path) { socket_.connectToServer(path); }
void Picker::send(const QByteArray &line) { socket_.write(line+'\n'); socket_.flush(); }
void Picker::setPreviewQuery(const QString &query) { search_->setText(query); refresh(); if (preview_) search_->setFocus(); }
void Picker::openSession(const QByteArray &token) {
    token_ = token; session_ = true; shiftHeld_ = false; collected_.clear(); search_->clear(); selectCategory(2); updateCollection();
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
