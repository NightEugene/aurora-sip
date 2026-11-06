#pragma once
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <cstdlib>
// Last-resort process deadline, only after the Qt event loop has exited.
// Never destroys live worker-owned objects or terminates individual threads.
class ShutdownGuard {
public:
 void arm(int milliseconds=5000){worker=std::thread([this,milliseconds]{std::unique_lock<std::mutex> lock(mutex);if(!done.wait_for(lock,std::chrono::milliseconds(milliseconds),[this]{return finished;}))std::_Exit(EXIT_FAILURE);});}
 ~ShutdownGuard(){ {std::lock_guard<std::mutex> lock(mutex);finished=true;}done.notify_all();if(worker.joinable())worker.join();}
private:
 std::mutex mutex;std::condition_variable done;bool finished=false;std::thread worker;
};
