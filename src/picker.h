// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractListModel>
#include <QColor>
#include <QListView>
#include <QLocalSocket>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QToolButton;
class QFrame;

struct Emoji {
    QString text, name, english, search;
    int category;
    QStringList tones;
    QStringList words;
    bool symbol = false;
};

struct Colors {
    QColor background, surface, sidebar, border, text, muted, accent, selected, hover;
};

class EmojiModel : public QAbstractListModel {
public:
    explicit EmojiModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex & = {}) const override { return visible.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    void refresh();
    QString glyph(int row) const;
    const Emoji *entry(int row) const;
    bool favorite(int row) const;
    QList<Emoji> entries;
    QList<int> visible;
    QStringList favorites, recents;
    int category = -1, tone = 0;
    QString query;
    bool symbols = false;
};

class EmojiDelegate : public QStyledItemDelegate {
public:
    explicit EmojiDelegate(EmojiModel *model, QObject *parent = nullptr) : QStyledItemDelegate(parent), model_(model) {}
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {42, 42}; }
    Colors colors;
private:
    EmojiModel *model_;
};

class Picker : public QWidget {
    Q_OBJECT
public:
    explicit Picker(const QString &dataPath, bool preview = false);
    void connectToAddon(const QString &path);
    bool catalogueLoaded() const { return !model_.entries.isEmpty(); }
    void setPreviewQuery(const QString &query);
    void setSymbolMode(bool symbols);
protected:
    bool eventFilter(QObject *, QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    void buildUi();
    void applyTheme();
    QStringList categoryNames() const;
    QStringList symbolCategoryNames() const;
    void refresh();
    void selectCategory(int index);
    void toggleSymbols();
    void selectTone(int tone);
    void toggleFavorite(int row);
    void choose(int row, bool collect = false);
    void commit();
    void cancel();
    void updateCollection();
    void saveHistory();
    void moveSelection(int delta);
    void handleKey(int sym, int states, const QString &text);
    void receive(const QByteArray &line);
    void send(const QByteArray &line);
    void openSession(const QByteArray &token);
    void trackHover(const QPoint &position);
    void showHoverName();
    void hideHoverName();
    EmojiModel model_;
    EmojiDelegate *delegate_ = nullptr;
    QListView *grid_ = nullptr;
    QLineEdit *search_ = nullptr;
    QLabel *hoverName_ = nullptr;
    QLabel *heading_, *empty_, *collectionLabel_;
    QFrame *settingsPanel_, *tonePanel_, *collectionPanel_;
    QToolButton *skinButton_, *modeButton_;
    QPushButton *insert_;
    QCheckBox *continuous_;
    QList<QPushButton *> categoryButtons_, symbolButtons_;
    QList<QToolButton *> toneButtons_;
    QList<QPair<QString, QString>> collected_;
    QLocalSocket socket_;
    QSettings settings_;
    QTimer debounce_;
    QTimer hoverTimer_;
    QByteArray input_, token_;
    Colors colors_;
    QString theme_;
    int navIndex_ = 2;
    int hoveredRow_ = -1;
    bool preview_ = false, session_ = false, shiftHeld_ = false, symbols_ = false;
};
