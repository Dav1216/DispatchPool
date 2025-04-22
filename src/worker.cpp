#include "worker.h"

#include <mqueue.h>
#include <unistd.h>

#include <iostream>

#include "messages.h"

namespace {
int fib(int n) {
    return (n <= 1) ? n : fib(n - 1) + fib(n - 2);
}
}  // namespace

void Worker::run(const char* req_queue_name, const char* resp_queue_name,
                 const char* ack_queue_name) {
    int pid = getpid();
    std::cout << "[Worker " << pid << "] Starting up\n";

    mqd_t req_q = mq_open(req_queue_name, O_RDONLY);
    mqd_t resp_q = mq_open(resp_queue_name, O_WRONLY);
    mqd_t ack_q = mq_open(ack_queue_name, O_WRONLY);

    if (req_q == (mqd_t)-1 || resp_q == (mqd_t)-1 || ack_q == (mqd_t)-1) {
        perror("mq_open (worker)");
        return;
    }

    MQ_REQUEST_MESSAGE_WORKER msg;
    while (true) {
        ssize_t n = mq_receive(req_q, (char*)&msg, sizeof(msg), nullptr);
        if (n >= 0) {
            if (msg.job == -2) {
                break;
            }

            // send ACK
            WorkerAck ack_msg{.pid = pid, .job_id = msg.job};
            if (mq_send(ack_q, (char*)&ack_msg, sizeof(ack_msg), 0) == -1) {
                perror("[Worker] mq_send (ack)");
            }
            int result = fib(msg.data);

            MQ_RESPONSE_MESSAGE resp{.job = msg.job, .result = result, .worker = pid};

            // simulate worker failure
            if (std::rand() % 3 == 0) {
                std::cout << "[Worker " << pid << "] Simulated crash!\n";
                int* crash = nullptr;
                *crash = 42;
            }

            if (mq_send(resp_q, (char*)&resp, sizeof(resp), 0) == -1) {
                perror("[Worker] mq_send (response)");
            }
        } else {
            perror("mq_receive (worker)");
        }
    }

    std::cout << "[Worker " << getpid() << "] Received shutdown signal\n";
    mq_close(req_q);
    mq_close(resp_q);
    mq_close(ack_q);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "[Worker] Expected request, response, and ack queue names\n";
        return 1;
    }

    Worker wp;
    wp.run(argv[1], argv[2], argv[3]);
    return 0;
}
