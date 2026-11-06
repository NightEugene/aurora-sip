#pragma once
#include <QThread>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <pjsua-lib/pjsua.h>

// Runs native network polling, IP changes and destruction outside the GUI.
// The Phone must outlive this thread; callbacks post immutable snapshots to Qt.
class SipThread : public QThread {
    Q_OBJECT
public:
    explicit SipThread(pjmedia_port *port,QObject *parent=nullptr):QThread(parent),audioPort(port){}
    SipThread(std::function<bool()> initialize,std::function<void(unsigned)> teardown,QObject *parent=nullptr):QThread(parent),audioPort(nullptr),initializer(std::move(initialize)),cleanup(std::move(teardown)){}
    void post(std::function<void()> task){std::lock_guard<std::mutex> lock(commandMutex);if(!stopping.load())commands.push_back(std::move(task));}
    void requestIpChange(){ipChange.store(true);}
    void stop(bool sendUnregister=false){destroyFlags.store(sendUnregister?PJSUA_DESTROY_NO_RX_MSG:PJSUA_DESTROY_NO_NETWORK);stopping.store(true);}
    bool changingIp() const {return ipBusy.load();}
    bool stoppingNow() const {return stopping.load();}
signals:
    void ipChangeFinished(int status);
protected:
    void run() override;
private:
    pjmedia_port *audioPort;
    std::function<bool()> initializer;
    std::function<void(unsigned)> cleanup;
    std::mutex commandMutex;
    std::vector<std::function<void()>> commands;
    std::atomic<unsigned> destroyFlags{PJSUA_DESTROY_NO_NETWORK};
    std::atomic_bool stopping{false},ipChange{false},ipBusy{false};
};
