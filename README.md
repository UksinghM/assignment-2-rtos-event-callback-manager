@'
# RTOS Event & Callback Manager (C)

A command-line C program that simulates an RTOS-style event system for a
payment soundbox. Events are pushed into a fixed-size FIFO queue and are
processed later by a dispatcher that calls the registered callback functions.

```
User Command (PUSH)
        |
        v
  Event Queue (FIFO, circular, 10 slots)
        |
        v
  Event Dispatcher  (PROCESS)
        |
        v
  Registered Callback  (function pointer)
        |
        v
  Payment / Network / Audio action (printed text)
```

---

## 1. Approach and Architecture

The program is split into five separate responsibilities. Each one only does
its own job.

| Part | Functions | Job |
|---|---|---|
| Event queue | `eventQueueInit`, `eventQueuePush`, `eventQueuePop`, `eventQueuePeek`, `eventQueueIsEmpty`, `eventQueueIsFull` | Store events in FIFO order |
| Callback registration | `registerEventCallback` | Link an event to a callback function |
| Dispatcher | `eventDispatcher`, `processAllEvents` | Remove events from the queue and call their callbacks |
| Command handling | `processCommand`, `cmdRegister`, `cmdPush`, `cmdStatus`, `cmdList` | Read and validate user commands |
| `main()` | `main` | Initialise the queue and read input lines. Nothing else. |

### Data structures

- `enum EventType` - the six events. `EVENT_COUNT` is the last value and
  equals the number of events, so it is used as the size of the lookup tables.
- `struct Event` - one event: `type` and `amount`.
- `typedef void (*EventCallback)(Event *event);` - the function pointer type.
- `struct EventQueue` - an array `items[10]` plus `head`, `tail` and `count`.

### Circular FIFO queue

The queue is a fixed array. No `malloc` is used.

- `tail` is where the next event is written. After a push: `tail = (tail + 1) % 10`.
- `head` is the oldest event. After a pop: `head = (head + 1) % 10`.
- `count` tells how many events are stored. Empty means `count == 0`,
  full means `count == 10`.
- The `% 10` makes the indexes wrap around, so freed slots are reused.
- A push on a full queue returns `false` and changes nothing, so existing
  events are never overwritten.

### Callback registry

`g_registry[EVENT_COUNT]` is an array of function pointers, one per event.
`NULL` means "no callback registered". `REGISTER` looks up the callback name
in `CALLBACK_TABLE` (name -> function pointer) and stores the pointer in
`g_registry[event]`.

### Dispatcher

`PROCESS` pops events one by one from the head of the queue. For each event
`eventDispatcher` reads `g_registry[event.type]`:
- if it is `NULL`, a warning is printed and no function is called,
- otherwise the callback is called exactly once.

Because each event is removed from the queue before it is dispatched, a
callback can never run twice for the same event. `PUSH` only adds to the
queue and never calls a callback.

## 2. Assumptions and Design Decisions

- **Callback must match the event.** Following the callback reference table,
  `paymentReceivedCallback` only works for `PAYMENT_RECEIVED`, `networkCallback`
  for the two network events, and so on. Registering a wrong pair shows an error.
- **Duplicate registration is rejected.** An event can have only one callback.
  Registering it again shows an error and keeps the first one.
- **Unregistered event is safe.** It is still queued. On `PROCESS` a warning is
  printed and no function pointer is called.
- **Queue capacity is 10.** Change `QUEUE_CAPACITY` to resize it.
- **Full queue rejects the new event.** The oldest events are kept.
- **Amount** can be a whole or decimal number, 0 or more (0 is needed for
  network and battery events). Negative numbers and text are rejected.
- Command names and event names are **case-insensitive**. Callback names are
  **case-sensitive** (they are C function names).
- `PROCESS` handles all queued events. When it finishes it prints `Queue empty`.
- Data lives only in memory. Blank lines are ignored.
- `PAYMENT_SUCCESS` and `PAYMENT_FAILED` callbacks print an information line
  and then a simulated audio line.

'@ | Out-File -Encoding ascii README.md
@'
## 3. Build and Run (step by step)

### What you need
A C compiler. Check with:
```
gcc --version
```
- **Windows:** install [MSYS2](https://www.msys2.org/) or MinGW-w64.
- **macOS:** run `xcode-select --install`.
- **Linux (Ubuntu/Debian):** run `sudo apt install build-essential`.

### Step 1 - Get the code
```
git clone https://github.com/UksinghM/assignment-2-rtos-event-callback-manager.git
cd assignment-2-rtos-event-callback-manager
```

### Step 2 - Compile
Linux / macOS / Git Bash:
```
gcc -std=c11 -Wall -Wextra -pedantic -o event_manager main.c -lm
```
Windows PowerShell:
```
gcc -std=c11 -Wall -Wextra -pedantic -o event_manager.exe main.c -lm
```

### Step 3 - Run
```
./event_manager          # Linux / macOS / Git Bash
.\event_manager.exe      # Windows PowerShell
```
The program waits for commands (no prompt is shown, this is normal).
Type `EXIT` to quit.

### Run the ready-made sample input
```
./event_manager < sample_input.txt                     # Linux / macOS / Git Bash
Get-Content sample_input.txt | .\event_manager.exe     # Windows PowerShell
```

## 4. Supported Commands

| Command | Syntax | Purpose |
|---|---|---|
| REGISTER | `REGISTER <event> <callback>` | Register a callback for an event |
| PUSH | `PUSH <event> <amount>` | Add an event to the queue (does not run the callback) |
| PROCESS | `PROCESS` | Remove all queued events in FIFO order and dispatch them |
| STATUS | `STATUS` | Show queue count and capacity |
| LIST | `LIST` | Show queued events without removing them |
| EXIT | `EXIT` | Quit |

### Events and their callbacks

| Event | Callback |
|---|---|
| `PAYMENT_RECEIVED` | `paymentReceivedCallback` |
| `PAYMENT_SUCCESS` | `paymentSuccessCallback` |
| `PAYMENT_FAILED` | `paymentFailedCallback` |
| `NETWORK_CONNECTED` | `networkCallback` |
| `NETWORK_DISCONNECTED` | `networkCallback` |
| `LOW_BATTERY` | `batteryCallback` |

'@ | Add-Content -Encoding ascii README.md
@'
## 5. Sample Run

Input (`sample_input.txt`):
```
REGISTER PAYMENT_RECEIVED paymentReceivedCallback
REGISTER PAYMENT_SUCCESS paymentSuccessCallback
REGISTER PAYMENT_FAILED paymentFailedCallback
REGISTER NETWORK_CONNECTED networkCallback
REGISTER NETWORK_DISCONNECTED networkCallback
REGISTER LOW_BATTERY batteryCallback

PUSH NETWORK_CONNECTED 0
PUSH PAYMENT_RECEIVED 500
PUSH PAYMENT_SUCCESS 500
PUSH PAYMENT_FAILED 200
PUSH LOW_BATTERY 0

STATUS
LIST
PROCESS
STATUS
EXIT
```

Output (also saved in `sample_output.txt`):
```
Callback registered for PAYMENT_RECEIVED
Callback registered for PAYMENT_SUCCESS
Callback registered for PAYMENT_FAILED
Callback registered for NETWORK_CONNECTED
Callback registered for NETWORK_DISCONNECTED
Callback registered for LOW_BATTERY
Event NETWORK_CONNECTED queued
Event PAYMENT_RECEIVED queued
Event PAYMENT_SUCCESS queued
Event PAYMENT_FAILED queued
Event LOW_BATTERY queued
Queue count: 5/10
Queued events:
NETWORK_CONNECTED | 0
PAYMENT_RECEIVED | 500
PAYMENT_SUCCESS | 500
PAYMENT_FAILED | 200
LOW_BATTERY | 0
[EVENT] NETWORK_CONNECTED
[NETWORK] Connected

[EVENT] PAYMENT_RECEIVED
[PAYMENT] Received: Rs. 500

[EVENT] PAYMENT_SUCCESS
[PAYMENT] Success: Rs. 500
[AUDIO] Playing: Payment Successful

[EVENT] PAYMENT_FAILED
[PAYMENT] Failed: Rs. 200
[AUDIO] Playing: Payment Failed

[EVENT] LOW_BATTERY
[WARNING] Low Battery

Queue empty
Queue count: 0/10
```

## 6. Test Cases Verified

| Test | Input | Result |
|---|---|---|
| Register and process one event | `REGISTER LOW_BATTERY batteryCallback`, `PUSH LOW_BATTERY 0`, `PROCESS` | Callback runs once |
| FIFO order | Push A, B, C then `PROCESS` | Dispatched A, B, C |
| LIST keeps events | `LIST` then `STATUS` | Count unchanged |
| PROCESS empties queue | `PROCESS` then `STATUS` | `Queue count: 0/10` |
| PROCESS on empty queue | `PROCESS` | `Queue empty`, no crash |
| Queue full | Push 11 events | 11th: `Error: queue full, event ... rejected`, old events intact |
| Duplicate registration | Register same event twice | `Error: callback already registered for ...` |
| Invalid event | `PUSH FOO 1` | `Error: invalid event FOO` |
| Invalid callback | `REGISTER LOW_BATTERY fooCallback` | `Error: invalid callback fooCallback` |
| Wrong callback for event | `REGISTER LOW_BATTERY networkCallback` | `Error: ... cannot handle LOW_BATTERY` |
| Unregistered event | `PUSH LOW_BATTERY 0`, `PROCESS` | Warning printed, no crash |
| Bad amount | `PUSH LOW_BATTERY abc` or `-5` | `Error: amount must be a number, 0 or more` |
| Unknown / malformed command | `HELLO`, `PUSH` | `Error: unknown command` / usage message |
| Wrap-around | Push 7, process, push 10 | All 10 stored and listed in correct order |

## 7. Project Files

```
main.c             all source code
sample_input.txt   example input from the assignment
sample_output.txt  output of that input
README.md          this file
.gitignore         keeps compiled programs out of the repository
```
'@ | Add-Content -Encoding ascii README.md