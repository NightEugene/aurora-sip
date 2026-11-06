#pragma once
#include <QVariantMap>
#include <QString>
namespace DtmfTone {
inline QVariantMap properties(const QString &digit){
    if(digit.size()!=1)return {};
    const QChar key=digit.at(0);quint32 code;
    if(key>='0'&&key<='9')code=key.unicode()-QChar('0').unicode();
    else if(key=='*')code=10;
    else if(key=='#')code=11;
    else return {};
    QVariantMap values;
    // NGFD reads this using n_proplist_get_uint(). An int32 becomes tone 0.
    values.insert(QStringLiteral("tonegen.value"),QVariant::fromValue(code));
    return values;
}
}
