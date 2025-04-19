#pragma once
#include <sys/types.h>  // for pid_t

struct WorkerInfo {
    pid_t pid;
    int id;  // worker index
    bool alive;
};

class Dealer {
   public:
    void run();
};
