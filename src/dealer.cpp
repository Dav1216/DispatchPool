#include "dealer.h"

#include <errno.h>
#include <mqueue.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "messages.h"

int signal_pipe[2];

std::atomic<int> jobs_sent = 0;
std::atomic<int> jobs_received = 0;

std::atomic<bool> generator_finished = false;
std::atomic<bool> shutting_down = false;

std::unordered_set<pid_t> workers;
std::unordered_map<pid_t, int> job_by_worker;
std::unordered_map<int, MQ_REQUEST_MESSAGE_WORKER> job_cache;

std::mutex mu;
std::queue<WorkerAck> ack_buffer;

// for the fault tolerance
void handle_sigchld(int) {
    char x = 'x';
    write(signal_pipe[1], &x, 1);  // async-signal-safe notification
}

void install_sigchld_handler() {
    struct sigaction sa{};
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("sigaction");
        exit(1);
    }
}

void setup_signal_pipe() {
    if (pipe(signal_pipe) == -1) {
        perror("pipe");
        exit(1);
    }
}

// thread 1
void ack_loop(mqd_t ack_q) {
    WorkerAck ack;
    while (true) {
        if (shutting_down.load()) break;
        ssize_t n = mq_receive(ack_q, (char*)&ack, sizeof(ack), nullptr);
        if (n >= 0) {
            std::lock_guard<std::mutex> lk(mu);
            ack_buffer.push(ack);
        } else if (errno != EAGAIN) {
            perror("[Dealer] mq_receive (ack)");
        }
    }
}

// must be called with mu locked to prevent races with ack_loop
void drain_ack_buffer() {
    while (!ack_buffer.empty()) {
        WorkerAck ack = ack_buffer.front();
        ack_buffer.pop();
        job_by_worker[ack.pid] = ack.job_id;
    }
}

// Thread 2: Handles SIGCHLD-driven crash recovery
// - Drains pending ACKs (buffer + MQ)
// - Looks up the job last assigned to the crashed worker
// - Resends it
// - Forks a replacement worker
void recovery_loop(const char* req_queue_name, const char* resp_queue_name,
                   const char* ack_queue_name, mqd_t req_q) {
    char buf;

    mqd_t ack_q = mq_open(ack_queue_name, O_RDONLY | O_NONBLOCK);

    while (read(signal_pipe[0], &buf, 1) > 0) {
        if (shutting_down.load()) break;

        int status;
        pid_t pid;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            std::cout << "[Supervisor] Worker PID " << pid << " exited with status " << status
                      << "\n";
            workers.erase(pid);

            {
                std::lock_guard<std::mutex> lk(mu);

                drain_ack_buffer();

                // Drain all pending ACKs before we process crashes
                WorkerAck ack;
                while (mq_receive(ack_q, (char*)&ack, sizeof(ack), nullptr) >= 0) {
                    job_by_worker[ack.pid] = ack.job_id;
                }

                // Check if that worker had a job
                auto it = job_by_worker.find(pid);
                if (it != job_by_worker.end()) {
                    int job_id = it->second;

                    auto msg_it = job_cache.find(job_id);
                    if (msg_it != job_cache.end()) {
                        MQ_REQUEST_MESSAGE_WORKER msg = msg_it->second;
                        if (mq_send(req_q, (char*)&msg, sizeof(msg), 0) == -1) {
                            perror("[Recovery] mq_send (resend)");
                        } else {
                            std::cout << "[Recovery] Resent job " << job_id << " after crash of "
                                      << pid << "\n";
                        }
                    }
                }
            }

            pid = fork();
            if (pid < 0) {
                std::cerr << "[Dealer] Failed to fork replacement for PID " << pid << "\n";
            } else if (pid == 0) {
                execlp("bin/worker", "worker", req_queue_name, resp_queue_name, ack_queue_name,
                       nullptr);
                perror("execlp failed");
                exit(1);
            } else {
                workers.insert(pid);
            }
        }
    }
}

// for the two threads no circular wait
// no thread blocks on one queue before operating on the other queue

// Thread 3: Sends jobs from the generator to workers
// - Populates job_cache to support resending
// - Ensures no duplicate jobs are counted or sent
void send_loop(mqd_t gen_q, mqd_t req_q) {
    MQ_REQUEST_MESSAGE_WORKER msg;

    while (true) {
        ssize_t gen_n = mq_receive(gen_q, (char*)&msg, sizeof(msg), nullptr);

        if (gen_n >= 0) {
            if (msg.job == -1) {
                generator_finished = true;
                break;
            }
            // Check if we've already seen this job, don't increment counter if we did
            if (job_cache.find(msg.job) != job_cache.end()) {
                std::cout << "[Dealer] Resent failed job " << msg.job << " -> fib(" << msg.data
                          << ")\n";
                msg = job_cache[msg.job];
                if (mq_send(req_q, (char*)&msg, sizeof(msg), 0) == -1) {
                    perror("[Dealer] mq_send (request)");
                }
                continue;
            } else {
                job_cache[msg.job] = msg;
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

// Thread 4: Receives completed results from workers
// - Increments jobs_received
// - Optional: cleans up job_by_worker to avoid stale entries
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

void spawn_workers(int num_workers, const char* req_queue_name, const char* resp_queue_name,
                   char* ack_queue_name) {
    int i = 0;

    while (i < num_workers) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[Dealer] Failed to fork worker " << i << "\n";
        } else if (pid == 0) {
            execlp("bin/worker", "worker", req_queue_name, resp_queue_name, ack_queue_name,
                   nullptr);
            perror("execlp failed");
            exit(1);
        } else {
            workers.insert(pid);
            i += 1;
        }
    }
}

void send_shutdown(mqd_t req_q) {
    MQ_REQUEST_MESSAGE_WORKER shutdown_msg{.job = -2, .data = 0};
    for (size_t i = 0; i < workers.size(); ++i) {
        mq_send(req_q, (char*)&shutdown_msg, sizeof(shutdown_msg), 0);
    }
}

void reap_workers() {
    std::cout << "[Dealer] All jobs completed. Shutting down workers...\n";

    for (const auto& pid : workers) {
        int status;
        waitpid(pid, &status, 0);
    }

    std::cout << "[Dealer] Shutting down.\n";
}

void Dealer::run(const char* queue_generator_name) {
    std::cout << "[Dealer] Starting" << std::endl;
    // setup
    setup_signal_pipe();
    install_sigchld_handler();

    char req_queue_name[64], resp_queue_name[64], ack_queue_name[64];
    snprintf(req_queue_name, sizeof(req_queue_name), "/tp_req_%d", getpid());
    snprintf(resp_queue_name, sizeof(resp_queue_name), "/tp_resp_%d", getpid());
    snprintf(ack_queue_name, sizeof(ack_queue_name), "/tp_ack_%d", getpid());

    mq_attr req_attr = {};
    mq_attr resp_attr = {};
    mq_attr ack_attr = {};

    req_attr.mq_flags = 0;
    req_attr.mq_maxmsg = 10;
    req_attr.mq_msgsize = sizeof(MQ_REQUEST_MESSAGE_WORKER);
    req_attr.mq_curmsgs = 0;

    resp_attr.mq_flags = 0;
    resp_attr.mq_maxmsg = 10;
    resp_attr.mq_msgsize = sizeof(MQ_RESPONSE_MESSAGE);
    resp_attr.mq_curmsgs = 0;

    ack_attr.mq_flags = 0;
    ack_attr.mq_maxmsg = 10;
    ack_attr.mq_msgsize = sizeof(WorkerAck);
    ack_attr.mq_curmsgs = 0;

    mqd_t req_q = mq_open(req_queue_name, O_CREAT | O_WRONLY, 0600, &req_attr);
    mqd_t resp_q = mq_open(resp_queue_name, O_CREAT | O_RDONLY, 0600, &resp_attr);
    mqd_t gen_q = mq_open(queue_generator_name, O_RDONLY);
    mqd_t ack_q = mq_open(ack_queue_name, O_CREAT | O_RDONLY, 0600, &ack_attr);

    if (resp_q == (mqd_t)-1 || gen_q == (mqd_t)-1 || req_q == (mqd_t)-1 || ack_q == (mqd_t)-1) {
        perror("[Dealer] mq_open failed");
        return;
    }

    const int num_workers = 4;
    spawn_workers(num_workers, req_queue_name, resp_queue_name, ack_queue_name);

    std::thread ack_reader(ack_loop, ack_q);
    ack_reader.detach();
    std::thread supervisor(recovery_loop, req_queue_name, resp_queue_name, ack_queue_name, req_q);
    std::thread sender(send_loop, gen_q, req_q);
    std::thread receiver(recv_loop, resp_q);

    sender.join();
    receiver.join();

    shutting_down.store(true);
    send_shutdown(req_q);
    supervisor.join();
    reap_workers();

    mq_close(req_q);
    mq_close(gen_q);
    mq_close(resp_q);
    mq_close(ack_q);
    mq_unlink(queue_generator_name);
    mq_unlink(req_queue_name);
    mq_unlink(resp_queue_name);
    mq_unlink(ack_queue_name);

    close(signal_pipe[0]);
    close(signal_pipe[1]);
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
