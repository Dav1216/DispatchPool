#include "task_generator.h"

#include <mqueue.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "messages.h"

void TaskGenerator::run(const char* queue_generator_name) {
    mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(MQ_REQUEST_MESSAGE_WORKER);
    attr.mq_curmsgs = 0;

    mqd_t req_q = mq_open(queue_generator_name, O_CREAT | O_WRONLY, 0600, &attr);
    if (req_q == (mqd_t)-1) {
        perror("mq_open request queue");
        exit(1);
    }

    std::cout << "[TaskGenerator] Queue created: " << queue_generator_name << std::endl;

    std::vector<MQ_REQUEST_MESSAGE_WORKER> tasks = {
        {.job = 1, .data = 35},  {.job = 2, .data = 36},  {.job = 3, .data = 37},
        {.job = 4, .data = 38},  {.job = 5, .data = 39},  {.job = 6, .data = 40},
        {.job = 7, .data = 35},  {.job = 8, .data = 36},  {.job = 9, .data = 37},
        {.job = 10, .data = 38}, {.job = 11, .data = 39}, {.job = 12, .data = 40},
        {.job = -1, .data = 0}  // to shut down dealer
    };

    for (const auto& task : tasks) {
        if (mq_send(req_q, reinterpret_cast<const char*>(&task), sizeof(task), 0) == -1) {
            perror("[TaskGenerator] mq_send failed");
        } else {
            std::cout << "[TaskGenerator] Sent job " << task.job << " → fib(" << task.data << ")\n";
        }

        usleep(100000);  // 0.1s
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "[TaskGenerator] Expected queue name\n";
        return 1;
    }

    TaskGenerator tg;
    tg.run(argv[1]);
    return 0;
}
