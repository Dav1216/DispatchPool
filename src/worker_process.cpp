#include "worker_process.h"

#include <signal.h>
#include <unistd.h>

#include <iostream>

void WorkerProcess::run() {
    std::cout << "[Worker " << getpid() << "] Starting up" << std::endl;
    raise(SIGSEGV);
}