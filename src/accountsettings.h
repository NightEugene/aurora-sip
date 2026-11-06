#pragma once
#include "appsettings.h"
#include <QRegularExpression>
namespace AccountSettings {
inline QVariantMap profile(QString host,QString domain,int port,QString user,bool srtp){
 return {{"host",host.trimmed()},{"domain",domain.trimmed()},{"port",port},{"user",user.trimmed()},{"srtp",srtp}};
}
inline QString validate(const QVariantMap &values){
 for(const auto &key:{"host","domain","port","user","srtp"})if(!values.contains(key))return QStringLiteral("Параметры аккаунта не заполнены");
 const QRegularExpression hostname("\\A[A-Za-z0-9.-]{1,253}\\z"),username("\\A[A-Za-z0-9_.+%-]{1,128}\\z");
 if(!hostname.match(values.value("host").toString()).hasMatch()||!hostname.match(values.value("domain").toString()).hasMatch()||!username.match(values.value("user").toString()).hasMatch()||values.value("port").toInt()<1||values.value("port").toInt()>65535||values.value("srtp").type()!=QVariant::Bool)return QStringLiteral("Проверьте сервер, домен, порт и имя пользователя");
 return {};
}
inline QString save(const QVariantMap &values){
 const auto error=validate(values);if(!error.isEmpty())return error;
 AppSettings settings;for(const auto &key:{"host","domain","port","user","srtp"})settings.setValue(key,values.value(key));settings.sync();
 return settings.status()==QSettings::NoError?QString():QStringLiteral("Не удалось сохранить параметры аккаунта");
}
inline QString save(QString host,QString domain,int port,QString user,bool srtp){return save(profile(host,domain,port,user,srtp));}
inline QVariantMap withLocalAudioPreference(QVariantMap profile){
 AppSettings settings;
 bool sameAccount=true;
 for(const auto &key:{"host","domain","port","user"})if(settings.value(key)!=profile.value(key))sameAccount=false;
 if(sameAccount&&settings.contains("srtp"))profile["srtp"]=settings.value("srtp").toBool();
 return profile;
}
inline QVariantMap current(){AppSettings settings;return profile(settings.value("host").toString(),settings.value("domain").toString(),settings.value("port",5061).toInt(),settings.value("user").toString(),settings.value("srtp",false).toBool());}
}
