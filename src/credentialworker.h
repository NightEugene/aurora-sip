#pragma once
#include <QThread>
#include <QVariantMap>
#include <QtCrypto>

// Uses the USERAPP token following the official Aurora KeyStore example.
// The event handler and crypto operations use separate worker threads.
class CredentialWorker : public QThread {
    Q_OBJECT
public:
    explicit CredentialWorker(QObject *parent=nullptr);
    ~CredentialWorker();
    void save(const QString &password,const QVariantMap &account);
    void load();
signals:
    void progress(const QString &message);
    void loaded(const QString &password,const QVariantMap &account);
    void completed(bool success, const QString &message);
protected:
    void run() override;
private:
    QThread *events=nullptr;
    QString pending;
    QVariantMap pendingAccount;
    bool writing=false;
};
