#pragma once
#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QSet>
#include <QDateTime>

class ContactSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool usingSavedCopy READ usingSavedCopy NOTIFY changed)
    Q_PROPERTY(QString cacheStatus READ cacheStatus NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
public:
    explicit ContactSource(QObject *parent=nullptr,int requestTimeoutMs=20000,int idleTimeoutMs=10000);
    QString source() const {return savedSource;}
    QString status() const {return message;}
    bool loading() const {return busy;}
    bool usingSavedCopy() const {return savedCopy;}
    QString cacheStatus() const;
    QVariantList contacts() const {return rows;}
    static QUrl sourceUrl(const QString &text);
    static QString dialNumber(const QString &text);
    static bool parseXml(const QByteArray &xml,QVariantList *contacts,QString *error);
    Q_INVOKABLE void configure(const QString &source);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void contactsChanged();
    void refreshFinished(bool success);
private:
    QString cachePath() const;
    bool loadCache();
    bool saveCache(const QByteArray &xml);
    bool haveSnapshot=false,savedCopy=false,persistedSnapshot=false;
    QDateTime snapshotAt;
    void loadUrl(const QUrl &url);
    void finish(QNetworkReply *reply);
    void fail(const QString &error);
    void accept(const QByteArray &data);
    QNetworkAccessManager network;
    QNetworkReply *pending=nullptr;
    QTimer timeout,idleTimeout;
    quint64 loadGeneration=0;
    QString savedSource,message;
    QVariantList rows;
    QByteArray received;
    QSet<QUrl> visited;
    bool busy=false;
};
