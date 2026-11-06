#include "credentialstore.h"
#include "accountsettings.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
CredentialStore::CredentialStore(QObject *parent,int timeoutMs):QObject(parent){
 deadline.setSingleShot(true);deadline.setInterval(timeoutMs);
 connect(&deadline,&QTimer::timeout,this,[this]{process.kill();finish(false,QStringLiteral("Keystore не ответил вовремя. Повторите операцию."));});
 connect(&process,&QProcess::readyReadStandardOutput,this,[this]{
  output+=process.readAllStandardOutput();
  if(output.size()>65536){process.kill();finish(false,QStringLiteral("Некорректный ответ Keystore"));}
 });
 connect(&process,&QProcess::readyReadStandardError,this,[this]{process.readAllStandardError();});
 connect(&process,static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
  if(!busy){emit finished();return;}
  output+=process.readAllStandardOutput();
  auto result=QJsonDocument::fromJson(output).object();
  const bool ok=status==QProcess::NormalExit&&code==0&&result.value("success").toBool();
  if(ok&&result.contains("password"))emit loaded(result.value("password").toString(),result.value("account").toObject().toVariantMap());
  finish(ok,result.value("message").toString(QStringLiteral("Операция Keystore не выполнена")));
 });
 connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(false,QStringLiteral("Не удалось запустить Keystore"));});
}
CredentialStore::~CredentialStore(){deadline.stop();if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(1000);}output.fill(0);}
void CredentialStore::save(const QString &password,const QVariantMap &account){start(true,password,account);}
void CredentialStore::load(){start(false,{},{});}
void CredentialStore::start(bool writing,const QString &password,const QVariantMap &account){
 if(busy||process.state()!=QProcess::NotRunning)return;
 QJsonObject request;request["writing"]=writing;request["password"]=password;request["account"]=QJsonObject::fromVariantMap(account);
 QByteArray payload=QJsonDocument(request).toJson(QJsonDocument::Compact);
 if(payload.size()>12000||(writing&&(password.isEmpty()||!AccountSettings::validate(account).isEmpty()))){emit completed(false,QStringLiteral("Проверьте параметры аккаунта и длину пароля"));emit finished();return;}
 output.fill(0);output.clear();busy=true;
 emit progress(QStringLiteral("Открытие Keystore…"));
 process.setProcessChannelMode(QProcess::SeparateChannels);
 process.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--credential-worker")});
 process.write(payload);payload.fill(0);process.closeWriteChannel();deadline.start();
}
void CredentialStore::finish(bool success,const QString &message){
 if(!busy)return;
 deadline.stop();busy=false;output.fill(0);output.clear();
 emit completed(success,message);emit finished();
}
