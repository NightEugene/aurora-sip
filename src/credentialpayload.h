#pragma once
#include "accountsettings.h"
#include <QJsonDocument>
#include <QJsonObject>
// This structure is INSIDE the GCM ciphertext: the password and its account
// cannot be mixed across interrupted saves. The outer envelope stays atomic.
namespace CredentialPayload {
inline QByteArray prefix(){return QByteArray::fromHex("ff4155524f52415349500200");}
inline QByteArray encode(const QString &password,const QVariantMap &account){
 QJsonObject object;object["password"]=password;object["account"]=QJsonObject::fromVariantMap(account);
 return prefix()+QJsonDocument(object).toJson(QJsonDocument::Compact);
}
inline bool decode(int version,const QByteArray &plain,QString *password,QVariantMap *account){
 password->clear();account->clear();
 if(version==1){
  // Old files contain a UTF-8 password. 0xff in the new prefix is not valid
  // UTF-8, so changing v2 to v1 cannot turn a framed record into a password.
  const auto text=QString::fromUtf8(plain);if(plain.isEmpty()||text.toUtf8()!=plain)return false;
  *password=text;return true;
 }
 if(version!=2||!plain.startsWith(prefix()))return false;
 const auto object=QJsonDocument::fromJson(plain.mid(prefix().size())).object();
 if(!object.value("password").isString()||!object.value("account").isObject()||object.value("password").toString().isEmpty())return false;
 const auto values=object.value("account").toObject().toVariantMap();if(!AccountSettings::validate(values).isEmpty())return false;
 *password=object.value("password").toString();*account=values;return true;
}
}
