#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <cstdint>
#include <qcontainerfwd.h>
#include <utility>
#include <vector>

struct TrackItem {
    int64_t id = 0;
    QString title;
    QString artist;
    QString durationText;
    int durationMs = 0;
    QString coverUrl;
    bool isFavorite = false;
};

class TrackListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        DurationTextRole,
        DurationMsRole,
        CoverUrlRole,
        IsFavoriteRole
    };

    explicit TrackListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= items_.size()) {
            return {};
        }
        const auto& item = items_[static_cast<size_t>(index.row())];
        switch (role) {
            case TrackIdRole:      return QVariant::fromValue(static_cast<qint64>(item.id));
            case TitleRole:        return item.title;
            case ArtistRole:       return item.artist;
            case DurationTextRole: return item.durationText;
            case DurationMsRole:   return item.durationMs;
            case CoverUrlRole:     return item.coverUrl;
            case IsFavoriteRole:   return item.isFavorite;
            default:               return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {TrackIdRole,      "trackId"},
            {TitleRole,        "title"},
            {ArtistRole,       "artist"},
            {DurationTextRole, "durationText"},
            {DurationMsRole,   "durationMs"},
            {CoverUrlRole,     "coverUrl"},
            {IsFavoriteRole,   "isFavorite"}
        };
    }

    void setItems(std::vector<TrackItem> items) {
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
    }

private:
    std::vector<TrackItem> items_;
};

struct PlaylistItem {
    int64_t id = 0;
    QString name;
    int trackCount = 0;
    int durationSecs = 0;
    QString coverUrl;
};

class PlaylistListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        PlaylistIdRole = Qt::UserRole + 1,
        NameRole,
        TrackCountRole,
        DurationSecsRole,
        CoverURLRole
    };

    explicit PlaylistListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= items_.size()) {
            return {};
        }
        const auto& item = items_[static_cast<size_t>(index.row())];
        switch (role) {
            case PlaylistIdRole: return QVariant::fromValue(static_cast<qint64>(item.id));
            case NameRole:       return item.name;
            case TrackCountRole: return item.trackCount;
            case DurationSecsRole: return item.durationSecs;
            case CoverURLRole: return item.coverUrl;
            default:             return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {PlaylistIdRole, "playlistId"},
            {NameRole,       "name"},
            {TrackCountRole, "trackCount"},
            {DurationSecsRole, "durationSecs"},
            {CoverURLRole, "coverUrl"}
        };
    }

    void setItems(std::vector<PlaylistItem> items) {
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
    }

private:
    std::vector<PlaylistItem> items_;
};

struct ArtistItem {
    int64_t id = 0;
    QString name;
    QString coverUrl;
    QString biography;
    int playCount = 0;
    int trackCount = 0;
};

class ArtistListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        ArtistIdRole = Qt::UserRole + 1,
        NameRole,
        CoverUrlRole,
        PlayCountRole,
        TrackCountRole,
        BiographyRole
    };

    explicit ArtistListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= items_.size()) {
            return {};
        }
        const auto& item = items_[static_cast<size_t>(index.row())];
        switch (role) {
            case ArtistIdRole:  return QVariant::fromValue(static_cast<qint64>(item.id));
            case NameRole:      return item.name;
            case CoverUrlRole:  return item.coverUrl;
            case PlayCountRole: return item.playCount;
            case TrackCountRole: return item.trackCount;
            case BiographyRole: return item.biography;
            default:            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {ArtistIdRole,  "artistId"},
            {NameRole,      "name"},
            {CoverUrlRole,  "coverUrl"},
            {PlayCountRole, "playCount"},
            {TrackCountRole, "trackCount"},
            {BiographyRole, "biography"}
        };
    }

    void setItems(std::vector<ArtistItem> items) {
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
    }

private:
    std::vector<ArtistItem> items_;
};

struct AlbumItem {
    int64_t id = 0;
    QString name;
    QString coverUrl;
    int trackCount = 0;
    QString artists;
    int64_t year;
};

class AlbumListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        AlbumIdRole = Qt::UserRole + 1,
        NameRole,
        CoverUrlRole,
        TrackCountRole,
        ArtistsRole,
        Year
    };

    explicit AlbumListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= items_.size()) {
            return {};
        }
        const auto& item = items_[static_cast<size_t>(index.row())];
        switch (role) {
            case AlbumIdRole:  return QVariant::fromValue(static_cast<qint64>(item.id));
            case NameRole:      return item.name;
            case CoverUrlRole:  return item.coverUrl;
            case TrackCountRole: return item.trackCount;
            case ArtistsRole: return item.artists;
            case Year: return QVariant::fromValue(static_cast<qint64>(item.year));
            default:            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {AlbumIdRole,  "albumId"},
            {NameRole,      "name"},
            {CoverUrlRole,  "coverUrl"},
            {TrackCountRole, "trackCount"},
            {ArtistsRole, "artists"},
            {Year, "year"}
        };
    }

    void setItems(std::vector<AlbumItem> items) {
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
    }

private:
    std::vector<AlbumItem> items_;
};