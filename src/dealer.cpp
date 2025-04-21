#include "dealer.h"

#include <errno.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <thread>
#include <unordered_set>

#include "messages.h"

std::atomic<int> jobs_sent = 0;
std::atomic<int> jobs_received = 0;
std::atomic<bool> generator_finished = false;

// no busy wait, send and receive are blocking if queue is full / empty

// for the two threads no circular wait
// no thread blocks on one queue before operating on the other queue

// thread 1
void send_loop(mqd_t gen_q, mqd_t req_q) {
    MQ_REQUEST_MESSAGE_WORKER msg;

    while (true) {
        ssize_t gen_n = mq_receive(gen_q, (char*)&msg, sizeof(msg), nullptr);

        if (gen_n >= 0) {
            if (msg.job == -1) {
                generator_finished = true;
                break;
            } else {
                if (mq_send(req_q, (char*)&msg, sizeof(msg), 0) == -1) {
                    perror("[Dealer] mq_send (request)");
                } else {
                    ++jobs_sent;
                    std::cout << "[Dealer] Sent job " << msg.job << " -> fib(" << msg.data << ")\n";
                }
            }
        } else if (errno != EAGAIN) {
            perror("[Dealer] mq_receive (generator)");
        }
    }
}

// thread 2
void recv_loop(mqd_t resp_q) {
    MQ_RESPONSE_MESSAGE result;

    while (true) {
        ssize_t res_n = mq_receive(resp_q, (char*)&result, sizeof(result), nullptr);
        if (res_n >= 0) {
            ++jobs_received;
            std::cout << "[Dealer] Received result for job " << result.job << ": " << result.result
                      << " from worker PID [" << result.worker << "]" << std::endl;

            if (generator_finished && jobs_received == jobs_sent) {
                break;
            }
        } else if (errno != EAGAIN) {
            perror("[Dealer] mq_receive (response)");
        }
    }
}

std::unordered_set<pid_t> workers;

void Dealer::run(const char* queue_generator_name) {
    std::cout << "[Dealer] Starting" << std::endl;

    char req_queue_name[64];
    snprintf(req_queue_name, sizeof(req_queue_name), "/tp_req_%d", getpid());

    mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(MQ_REQUEST_MESSAGE_WORKER);
    attr.mq_curmsgs = 0;

    char resp_queue_name[64];
    snprintf(resp_queue_name, sizeof(resp_queue_name), "/tp_resp_%d", getpid());

    mq_attr resp_attr{};
    resp_attr.mq_flags = 0;
    resp_attr.mq_maxmsg = 10;
    resp_attr.mq_msgsize = sizeof(MQ_RESPONSE_MESSAGE);
    resp_attr.mq_curmsgs = 0;

    mqd_t req_q = mq_open(req_queue_name, O_CREAT | O_WRONLY, 0600, &attr);
    if (req_q == (mqd_t)-1) {
        perror("mq_open request queue");
        exit(1);
    }

    mqd_t resp_q = mq_open(resp_queue_name, O_CREAT | O_RDONLY, 0600, &resp_attr);
    if (resp_q == (mqd_t)-1) {
        perror("mq_open response queue");
        exit(1);
    }

    const int num_workers = 4;
    int i = 0;

    while (i < num_workers) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[Dealer] Failed to fork worker " << i << "\n";
        } else if (pid == 0) {
            execlp("bin/worker", "worker", req_queue_name, resp_queue_name, nullptr);
            perror("execlp failed");
            exit(1);
        } else {
            workers.insert(pid);
            i += 1;
        }
    }

    mqd_t gen_q = mq_open(queue_generator_name, O_RDONLY);
    if (gen_q == (mqd_t)-1) {
        perror("mq_open (dealer)");
        return;
    }

    std::thread sender(send_loop, gen_q, req_q);
    std::thread receiver(recv_loop, resp_q);

    sender.join();
    receiver.join();

    MQ_REQUEST_MESSAGE_WORKER shutdown_msg{.job = -2, .data = 0};
    for (size_t i = 0; i < workers.size(); ++i) {
        mq_send(req_q, (char*)&shutdown_msg, sizeof(shutdown_msg), 0);
    }

    std::cout << "[Dealer] All jobs completed. Shutting down workers...\n";

    for (const auto& pid : workers) {
        int status;
        waitpid(pid, &status, 0);
    }

    std::cout << "[Dealer] Shutting down.\n";

    mq_close(req_q);
    mq_close(gen_q);
    mq_close(resp_q);
    mq_unlink(queue_generator_name);
    mq_unlink(req_queue_name);
    mq_unlink(resp_queue_name);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "[Dealer] Expected queue name as argument\n";
        return 1;
    }

    Dealer dl;
    dl.run(argv[1]);
    return 0;
}
