#include "../src/shutdownguard.h"
#include <sys/wait.h>
#include <unistd.h>
#include <cassert>
int main(){
 {ShutdownGuard guard;guard.arm(100);}
 const pid_t child=fork();assert(child>=0);
 if(child==0){ShutdownGuard guard;guard.arm(100);std::this_thread::sleep_for(std::chrono::seconds(10));std::_Exit(0);}
 int status=0;assert(waitpid(child,&status,0)==child);assert(WIFEXITED(status));assert(WEXITSTATUS(status)==EXIT_FAILURE);
}
