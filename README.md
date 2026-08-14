# Synchronization with Semaphores Cats & Dogs Problem

An Operating Systems  assignment implementing **"Cats and Dogs" (pet shop) synchronization problem** — a variant of readers-writers with starvation prevention — using POSIX semaphores. Two versions are provided: one using threads within a single process, and one using separate processes with System V shared memory. A full write-up and analysis is included in [`Report.pdf`](Report.pdf).

## The Problem

A pet shop has a service room with limited capacity. Dogs and cats can be served, but:
- Dogs and cats can never be in the room at the same time (mutual exclusion between species)
- Multiple animals of the *same* species can be served concurrently, up to room capacity
- To prevent starvation, after **5 dogs** have been served consecutively, priority is handed to waiting cats for one batch, then priority returns to dogs

Each dog/cat is simulated with a random arrival delay and a fixed service time, and the program reports how the room fills, empties, and hands off priority between species in real time.

## Programs

| File | Concurrency model | Description |
|---|---|---|
| [`hw2a.c`](hw2a.c) | POSIX threads | Each dog/cat is a `pthread`. Synchronization uses `sem_t` (unnamed, process-local) semaphores: a `mutex` guarding shared state, plus `dogs_queue`/`cats_queue` semaphores each species blocks on while waiting for room access |
| [`hw2b.c`](hw2b.c) | Processes + shared memory | Each dog/cat is a forked child process. Shared state (`shared_state` struct) and semaphores live in System V shared memory segments (`shmget`/`shmat`), allowing the same synchronization logic to work across process boundaries |

Both implement identical entry/exit logic (`dog_wants_service`, `dog_leaves`, `cat_wants_service`, `cat_leaves`) and the same batch-quota starvation-prevention rule — the difference is purely in the concurrency primitive (threads + in-process semaphores vs. processes + shared-memory semaphores).

## Requirements

- GCC
- POSIX threads and semaphores (`pthread.h`, `semaphore.h`) — Linux/Unix

## Building

```bash
gcc -o hw2a hw2a.c -lpthread
gcc -o hw2b hw2b.c
```

> **Known issue:** as currently committed, `hw2b.c` does not compile — the shared-memory ID variable declarations (`shm_state_id`, `shm_mutex_id`, `shm_dogs_queue_id`) on lines 31–33 are missing semicolons, which cascades into "undeclared identifier" errors throughout `main()` and `cleanup_resources()`. Adding the missing semicolons after each declaration resolves it; `hw2a.c` compiles cleanly as-is.

## Usage

Both programs prompt interactively for the simulation parameters:

```bash
./hw2a
Enter service room capacity (MAXIMUM > 0): 3
Enter number of dogs (>= 0): 6
Enter number of cats (>= 0): 4
```

The simulation then runs, printing each pet's arrival, entry, and departure along with the current room occupancy, followed by the total simulation time:

```
Program took: 4.512 seconds
```

At the end, you're asked whether to run another simulation (`y`/`n`) without restarting the program.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
