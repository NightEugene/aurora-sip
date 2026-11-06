#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
// Runs provider operations in an isolated process: timeouts never terminate
// a thread inside the GUI process or destroy objects still in use by QCA.
class CredentialStore : public QObject {
 Q_OBJECT
public:
 explicit CredentialStore(QObject *parent=nullptr,int timeoutMs=30000);
 ~CredentialStore();
 bool isRunning() const {return busy||process.state()!=QProcess::NotRunning;}
 void save(const QString &password,const QVariantMap &account);
 void load();
 static int workerMain(int argc,char **argv);
signals:
 void progress(const QString &message);
 void loaded(const QString &password,const QVariantMap &account);
 void completed(bool success,const QString &message);
 void finished();
private:
 void start(bool writing,const QString &password,const QVariantMap &account);
 void finish(bool success,const QString &message);
 QProcess process;
 QTimer deadline;
 QByteArray output;
 bool busy=false;
};
