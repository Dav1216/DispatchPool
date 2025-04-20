// Dispatcher logic (queueing, assignment, monitoring)

#include "dealer.h"

#include <mqueue.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <unordered_map>
#include <vector>

#include "messages.h"
#include "worker_process.h"

// since I need atomicity
constexpr int MAX_DEAD_WORKERS = 64;
volatile sig_atomic_t dead_pids[MAX_DEAD_WORKERS];
volatile sig_atomic_t dead_count = 0;
std::unordered_map<pid_t, WorkerInfo> workers;

void handle_sigchld(int) {
    // Reap all dead children
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (dead_count < MAX_DEAD_WORKERS) {
            dead_pids[dead_count++] = pid;
            std::cout << "[SIGCHLD] Child " << pid << " exited with status " << status << "\n";
        } else {
            std::cout << "Overflow";
        }
    }
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

void recover_lost_nodes() {
    while (dead_count > 0) {
        pid_t pid = dead_pids[--dead_count];
        auto it = workers.find(pid);

        if (it != workers.end()) {
            WorkerInfo info = it->second;
            std::cout << "[Dealer] Respawning worker #" << info.id << " (old PID " << pid << ")\n";

            pid_t new_pid = fork();
            if (new_pid == 0) {
                execlp("bin/worker", "worker", info.queue_name.c_str(), nullptr);
                perror("execlp failed");
                exit(1);
            }

            info.pid = new_pid;
            info.alive = true;

            workers.erase(pid);
            workers.emplace(new_pid, info);

            std::cout << "[Dealer] Worker #" << info.id << " is now PID " << new_pid << "\n";
        } else {
            std::cout << "[Dealer] Could not find dead worker PID: " << pid << "\n";
        }
    }
}

void Dealer::run() {
    install_sigchld_handler();

    // Create job request queue (Dealer -> Workers)
    char req_queue_name[32];
    snprintf(req_queue_name, sizeof(req_queue_name), "/tp_req_%d", getpid());

    mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(MQ_REQUEST_MESSAGE_WORKER);
    attr.mq_curmsgs = 0;

    mqd_t req_q = mq_open(req_queue_name, O_CREAT | O_WRONLY, 0600, &attr);
    if (req_q == (mqd_t)-1) {
        perror("mq_open request queue");
        exit(1);
    }

    std::cout << "[Dealer] Opened job queue: " << req_queue_name << std::endl;
    std::string queue_name = req_queue_name;
    const int num_workers = 4;

    for (int i = 0; i < num_workers; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[Dealer] Failed to fork worker " << i << "\n";
        } else if (pid == 0) {
            execlp("bin/worker", "worker", queue_name.c_str(), nullptr);
            perror("execlp failed");
            exit(1);
        } else {
            workers.emplace(pid, WorkerInfo{pid, i, true, queue_name});
            std::cout << "[Dealer] Spawned worker #" << i << " with PID " << pid << "\n";
        }
    }

    int next_job_id = 0;

    while (true) {
        recover_lost_nodes();

        MQ_REQUEST_MESSAGE_WORKER msg;
        msg.job = next_job_id++;
        msg.data = 40 + (std::rand() % 5);

        if (mq_send(req_q, (char*)&msg, sizeof(msg), 0) == -1) {
            perror("[Dealer] mq_send failed");
        } else {
            std::cout << "[Dealer] Sent job " << msg.job << " -> fib(" << msg.data << ")\n";
        }

        sleep(1);  // One task per second
    }

    std::cout << "[Dealer] All workers have exited.\n";
}