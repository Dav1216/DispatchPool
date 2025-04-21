#pragma once

class Worker {
   public:
    void run(const char* req_queue_name, const char* resp_queue_name);
};