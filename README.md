# Fault-tolerant distributed task pool in modern C++ with POSIX message queues and signal-driven crash recovery

A fault-tolerant task dispatcher written from scratch in C++ using forked UNIX processes and POSIX message queues. It distributes CPU-bound jobs (like `fib(n)`) to worker processes. If a worker crashes mid-task, the system detects the failure via `SIGCHLD`, resends the job, and spawns a replacement — ensuring no job is lost.

---

## 🧱 Architecture

- `main.cpp`: Forks and launches both the `TaskGenerator` and the `Dealer`
- `task_generator.cpp`: Enqueues a fixed set of Fibonacci jobs
- `dealer.cpp`: Multi-threaded; distributes jobs to workers, collects results, and recovers from crashes
- `worker.cpp`: Sends ACKs after receiving jobs, computes `fib(n)`, and sends results (also simulates node failure via random crashes)

---

##  Correctness Model & Assumptions

- **Job IDs are unique.** The system uses job IDs as keys in caches and tracking maps.
- **One job per worker at a time.** The system assumes each worker only handles one job between ACK and result.
- **ACKs are always sent before compute.** If a worker crashes mid-task, its last ACKed job is recoverable.
- **No race conditions or deadlocks.**  
All shared state accessed by multiple threads (e.g., job ownership, result counters, ACK buffers) is atomic or mutex-protected.

The **job cache (`job_cache`) is thread-safe by design**, even though it is not guarded by a mutex. This is safe because:

  - Jobs are inserted into the cache **before they are sent** to any worker.
  - The recovery thread only reads from the cache **after** a worker has ACKed the job.
  - This creates a **happens-before guarantee** between write and read, making the pattern race-free without explicit synchronization.
The `send_loop` and `recv_loop` run in separate threads, ensuring that no thread blocks on both the request and response queues.  
This prevents the classic deadlock where:
    - The Dealer blocks on sending to a full request queue
    - Workers block on sending to a full response queue
    - But the Dealer never consumes from the response queue because it's stuck on the request queue.
  - By separating send and receive into independent threads, the system always makes progress even under full queues.
- **No job loss.** Every job is guaranteed to complete (at least once), even if the assigned worker crashes.

---

## ✨ Cool Stuff

- **No busy waiting**: Each thread blocks efficiently on `mq_receive` or pipe reads
- **Crash recovery**: If a worker fails, the Dealer:
  1. Detects the crash via `SIGCHLD`
  2. Resends the last job owned by that worker (based on ACKs)
  3. Forks a new worker to maintain pool size

---

## 🛠️ Build and Run

In a Linux environment or a VSCode Dev Container (provided via `.devcontainer.json`) and `Dockerfile`, run:

```bash
make
make run
make clean
```

## Sample output for jobs 1-12
```
[TaskGenerator] Queue created: /tp_gen_1465
[Dealer] Starting
[Dealer] Sent job 1 -> fib(40)
[Worker 1470] Starting up
[Worker 1471] Starting up
[Worker 1468] Starting up
[Worker 1469] Starting up
[Dealer] Sent job 2 -> fib(41)
[Dealer] Sent job 3 -> fib(42)
[Dealer] Sent job 4 -> fib(43)
[Dealer] Sent job 5 -> fib(44)
[Dealer] Sent job 6 -> fib(45)
[Dealer] Sent job 7 -> fib(45)
[Dealer] Received result for job 1: 102334155 from worker PID [1470]
[Dealer] Sent job 8 -> fib(41)
[Dealer] Sent job 9 -> fib(42)
[Dealer] Sent job 10 -> fib(43)
[Dealer] Sent job 11 -> fib(44)
[Dealer] Received result for job 2: 165580141 from worker PID [1471]
[Dealer] Sent job 12 -> fib(45)
[Dealer] Received result for job 3: 267914296 from worker PID [1468]
[Dealer] Received result for job 4: 433494437 from worker PID [1469]
[Dealer] Received result for job 8: 165580141 from worker PID [1469]
[Dealer] Received result for job 5: 701408733 from worker PID [1470]
[Worker 1469] Simulated crash!
[Supervisor] Worker PID 1469 exited with status 11
[Recovery] Resent job 9 after crash of 1469
[Worker 1558] Starting up
[Worker 1470] Simulated crash!
[Supervisor] Worker PID 1470 exited with status 11
[Recovery] Resent job 10 after crash of 1470
[Worker 1564] Starting up
[Dealer] Received result for job 6: 1134903170 from worker PID [1471]
[Dealer] Received result for job 7: 1134903170 from worker PID [1468]
[Worker 1471] Simulated crash!
[Supervisor] Worker PID 1471 exited with status 11
[Recovery] Resent job 9 after crash of 1471
[Worker 1586] Starting up
[Dealer] Received result for job 11: 701408733 from worker PID [1558]
[Dealer] Received result for job 9: 267914296 from worker PID [1586]
[Worker 1468] Simulated crash!
[Supervisor] Worker PID 1468 exited with status 11
[Recovery] Resent job 10 after crash of 1468
[Worker 1608] Starting up
[Dealer] Received result for job 10: 433494437 from worker PID [1558]
[Dealer] Received result for job 12: 1134903170 from worker PID [1564]
[Worker 1558] Received shutdown signal
[Worker 1586] Received shutdown signal
[Worker 1608] Received shutdown signal
[Worker 1564] Received shutdown signal
[Dealer] All jobs completed. Shutting down workers...
[Dealer] Shutting down.
[Main] All done.
```