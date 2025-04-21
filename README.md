# Process Pool with POSIX Message Queues


This project implements a simple and correct task processing system using **UNIX processes** and **POSIX message queues**. It demonstrates real-world interprocess communication (IPC), explicit process management, and a clean shutdown model.

---

## Architecture
- `main.cpp`: forks and launches both the `TaskGenerator` and `Dealer`
- `task_generator.cpp`: enqueues a fixed set of Fibonacci jobs
- `dealer.cpp`: distributes jobs to worker processes and collects results
- `worker.cpp`: computes `fib(n)` and sends results back

## Build and Run

```bash
make
make run
make clean
```
---

## Sample Output

```
[Dealer] Received result for job 8: 14930352 from worker PID [505]
[Dealer] Sent job 11 -> fib(39)
```
