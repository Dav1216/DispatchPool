#pragma once
#include <sys/types.h>
#include <string>
  // for pid_t

struct WorkerInfo {
    pid_t pid;
    int id;  // worker index
    bool alive;
    std::string queue_name;
};

class Dealer {
   public:
    void run();
};
