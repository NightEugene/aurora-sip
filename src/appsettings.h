#pragma once
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

// Keep settings inside the application's whitelisted configuration directory.
// Cache the path before creating the QML view so all subsystems use one file.
class AppSettings : public QSettings {
public:
    static QString dataDirectory(){
        static const QString dir=[] {QString value=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(value);QFile::setPermissions(value,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return value;}();
        return dir;
    }
    AppSettings():QSettings(path(),QSettings::IniFormat){}
private:
    static QString path(){
        static const QString file=[] {
            const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(dir);
            QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
            const QString target=dir+"/settings.ini";
            const QString legacy=dir+"/ru.nighteugene.aurorasip.conf";
            if(!QFile::exists(target)&&QFile::exists(legacy))QFile::copy(legacy,target);
            return target;
        }();
        return file;
    }
};
