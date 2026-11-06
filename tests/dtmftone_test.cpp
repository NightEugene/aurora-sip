#include "dtmftone.h"
#include <QtTest>
#include <QDBusMetaType>
class DtmfToneTest : public QObject {
    Q_OBJECT
private slots:
    void eachKeyUsesDistinctUnsignedWireCode(){
        const QString keys="0123456789*#";QSet<quint32> codes;
        for(int i=0;i<keys.size();++i){
            auto values=DtmfTone::properties(keys.mid(i,1));QCOMPARE(values.size(),1);
            const auto value=values.value("tonegen.value");
            QCOMPARE(value.userType(),int(QMetaType::UInt));
            QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(value.userType())),QByteArray("u"));
            QCOMPARE(value.toUInt(),quint32(i));codes.insert(value.toUInt());
        }
        QCOMPARE(codes.size(),12);
    }
    void unsupportedInputHasNoTone(){
        for(auto key:{QString(),QString("12"),QString("+"),QString("A"),QString("x")})QVERIFY(DtmfTone::properties(key).isEmpty());
    }
};
QTEST_GUILESS_MAIN(DtmfToneTest)
#include "dtmftone_test.moc"
