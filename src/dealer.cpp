// Dispatcher logic (queueing, assignment, monitoring)

#include "dealer.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "worker_process.h"

// std::unordered_map<pid_t, WorkerInfo> workers;
// because it becomes concurrent when the signal is invoked
// volatile sig_atomic_t dead_worker_flag = 0;
// pid_t last_dead_pid = -1;

void handle_sigchld(int) {
    // Reap all dead children
    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);  // Don't block
        if (pid <= 0) break;

        std::cout << "[SIGCHLD] Child " << pid << " exited with status " << status << "\n";

        // last_dead_pid = pid;
        // dead_worker_flag = 1; // Notify main loop
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

// void recover_lost_nodes() {
//     if (!dead_worker_flag) return;
//     auto it = workers.find(last_dead_pid);

//     if (it != workers.end()) {
//         WorkerInfo& info = it->second;
//         std::cout << "[Dealer] Respawning worker #" << info.id << " (old PID " << info.pid <<
//         ")\n";

//         pid_t new_pid = fork();
//         if (new_pid == 0) {
//             WorkerProcess wp;
//             wp.run();
//             exit(0);
//         }

//         info.pid = new_pid;
//         info.alive = true;

//         // Update map: remove old, insert new
//         workers.erase(last_dead_pid);
//         workers[new_pid] = info;

//         std::cout << "[Dealer] Worker #" << info.id << " is now PID " << new_pid << "\n";
//     }

//     dead_worker_flag = 0;
//     last_dead_pid = -1;
// }

void Dealer::run() {
    install_sigchld_handler();
    std::vector<WorkerInfo> workers;
    const int num_workers = 4;

    for (int i = 0; i < num_workers; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[Dealer] Failed to fork worker " << i << "\n";
        } else if (pid == 0) {
            WorkerProcess wp;
            wp.run();
            exit(0);
        } else {
            workers.push_back(WorkerInfo{pid, i, true});
            std::cout << "[Dealer] Spawned worker #" << i << " with PID " << pid << "\n";
        }
    }
    usleep(1000000);

    std::cout << "[Dealer] All workers have exited.\n";
}