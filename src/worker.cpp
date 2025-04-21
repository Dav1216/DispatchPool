#include "worker.h"

#include <mqueue.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>

#include "messages.h"

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

void Worker::run(const char* queue_name) {
    std::cout << "[Worker " << getpid() << "] Starting up\n";

    mqd_t req_q = mq_open(queue_name, O_RDONLY);
    if (req_q == (mqd_t)-1) {
        perror("mq_open (worker)");
        return;
    }

    MQ_REQUEST_MESSAGE_WORKER msg;
    while (true) {
        ssize_t n = mq_receive(req_q, (char*)&msg, sizeof(msg), nullptr);
        if (n >= 0) {
            std::cout << "[Worker " << getpid() << "] Job " << msg.job << ": fib(" << msg.data
                      << ")\n";
            int result = fib(msg.data);
            std::cout << "[Worker " << getpid() << "] Result: " << result << "\n";

            if ((std::rand() % 5) == 0) {  // randomly crash a worker
                std::cerr << "[Worker " << getpid() << "] Crashing on purpose!\n";
                raise(SIGSEGV);
            }

        } else {
            perror("mq_receive (worker)");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "[Worker] Expected queue name as argument\n";
        return 1;
    }

    Worker wp;
    wp.run(argv[1]);
    return 0;
}
