#pragma once
#include <QObject>
#include <QVariantList>
#include <QSet>
#include <QHash>
#include <QThread>
#include <QTimer>
#include "contactsource.h"

class FavoriteWriter : public QObject {
    Q_OBJECT
public slots:
    void write(const QVariantList &rows);
signals:
    void stored(bool success);
};

class AddressBook : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY contactsChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(int namesRevision READ namesRevision NOTIFY namesChanged)
    Q_PROPERTY(int favoritesRevision READ favoritesRevision NOTIFY favoritesChanged)
    Q_PROPERTY(ContactSource* directory READ directory CONSTANT)
    Q_PROPERTY(QString storageError READ storageError NOTIFY storageErrorChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY contactsChanged)
    Q_PROPERTY(QString status READ status NOTIFY contactsChanged)
public:
    explicit AddressBook(QObject *parent=nullptr);
    ~AddressBook();
    QVariantList contacts() const {return directorySource.contacts();}
    ContactSource* directory(){return &directorySource;}
    QVariantList favorites() const {return savedFavorites;}
    int namesRevision() const {return nameRevision;}
    Q_INVOKABLE QString nameForNumber(const QString &number) const;
    int favoritesRevision() const {return revision;}
    QString storageError() const {return writeError;}
    bool loading() const {return directorySource.loading();}
    QString status() const {return directorySource.status();}
    Q_INVOKABLE void retrySave(){emit persistRequested(savedFavorites);}
    Q_INVOKABLE void refresh(){directorySource.refresh();}
    Q_INVOKABLE QString dialNumber(const QString &value) const;
    Q_INVOKABLE bool isFavorite(const QString &number) const;
    Q_INVOKABLE void toggleFavorite(const QString &name,const QString &number);
signals:
    void contactsChanged();
    void favoritesChanged();
    void storageErrorChanged();
    void namesChanged();
    void persistRequested(const QVariantList &rows);
private:
    QString writeError;
    void rebuildNames();
    QHash<QString,QString> contactNames;
    int nameRevision=0;
    ContactSource directorySource;
    QVariantList savedFavorites;
    QSet<QString> favoriteNumbers;
    int revision=0;
    QThread storageThread;
    FavoriteWriter *writer=nullptr;
    QTimer persistTimer;
};
