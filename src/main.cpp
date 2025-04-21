// Entry point (initializes and runs Dispatcher)
#include <unistd.h>

#include <iostream>

#include "task_generator.h"

int main() {
    char queue_name[64];
    snprintf(queue_name, sizeof(queue_name), "/tp_gen_%d", getpid());

    pid_t tg_pid = fork();
    if (tg_pid == 0) {
        execlp("bin/task_generator", "task_generator", queue_name, nullptr);
        perror("execlp (task_generator)");
        exit(1);
    }

    pid_t dealer_pid = fork();
    if (dealer_pid == 0) {
        execlp("bin/dealer", "main", queue_name, nullptr);
        perror("execlp (dealer)");
        exit(1);
    }
}
