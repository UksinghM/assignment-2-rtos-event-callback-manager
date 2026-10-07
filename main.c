/*
 * RTOS Event & Callback Manager
 * -----------------------------
 * Simulates an RTOS-style event system for a payment soundbox.
 *
 * Flow:  command -> Event Queue (FIFO) -> Dispatcher -> Registered Callback
 *
 * Commands: REGISTER <event> <callback> | PUSH <event> <amount>
 *           PROCESS | STATUS | LIST | EXIT
 *
 * Responsibilities are kept separate:
 *   1. Event queue        - eventQueueInit/Push/Pop/Peek/IsEmpty/IsFull
 *   2. Callback registry  - registerEventCallback
 *   3. Dispatcher         - eventDispatcher, processAllEvents
 *   4. Command handling   - processCommand and the cmd* helpers
 *   5. main()             - only initialises and reads input
 */

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_CAPACITY 10   /* requirement: at least 10 */
#define LINE_SIZE      256
#define NAME_BUF_SIZE  64

/* ====================== Types ====================== */

typedef enum {
    EVENT_PAYMENT_RECEIVED,
    EVENT_PAYMENT_SUCCESS,
    EVENT_PAYMENT_FAILED,
    EVENT_NETWORK_CONNECTED,
    EVENT_NETWORK_DISCONNECTED,
    EVENT_LOW_BATTERY,
    EVENT_COUNT                 /* always last: number of event types */
} EventType;

typedef struct {
    EventType type;
    double    amount;
} Event;

/* Function pointer type for every callback. */
typedef void (*EventCallback)(Event *event);

/* Fixed-size circular FIFO queue. No dynamic memory. */
typedef struct {
    Event items[QUEUE_CAPACITY];
    int   head;    /* index of the oldest event (next to pop)  */
    int   tail;    /* index where the next event will be added */
    int   count;   /* number of events currently stored        */
} EventQueue;

typedef enum {
    REGISTER_OK,
    REGISTER_UNKNOWN_CALLBACK,
    REGISTER_WRONG_EVENT,
    REGISTER_DUPLICATE
} RegisterResult;

typedef struct {
    const char   *name;
    EventCallback function;
} CallbackEntry;

/* ====================== Lookup tables ====================== */

static const char *const EVENT_NAMES[EVENT_COUNT] = {
    "PAYMENT_RECEIVED",
    "PAYMENT_SUCCESS",
    "PAYMENT_FAILED",
    "NETWORK_CONNECTED",
    "NETWORK_DISCONNECTED",
    "LOW_BATTERY"
};

/* Which callback name belongs to which event (from the assignment). */
static const char *const EXPECTED_CALLBACK[EVENT_COUNT] = {
    "paymentReceivedCallback",
    "paymentSuccessCallback",
    "paymentFailedCallback",
    "networkCallback",
    "networkCallback",
    "batteryCallback"
};

/* ====================== Global state ====================== */

static EventQueue    g_queue;
static EventCallback g_registry[EVENT_COUNT];   /* NULL = not registered */

/* ====================== Small helpers ====================== */

static void printAmount(double amount)
{
    if (amount == floor(amount) && amount < 1e15)
        printf("%.0f", amount);
    else
        printf("%.2f", amount);
}

/* Parse a number that is zero or more (0 is allowed, e.g. network events). */
static bool parseAmount(const char *text, double *out)
{
    char  *end   = NULL;
    double value = strtod(text, &end);

    if (end == text || *end != '\0') return false;
    if (!isfinite(value))            return false;
    if (value < 0)                   return false;

    *out = value;
    return true;
}

/* Convert text like "payment_received" into an EventType. */
static bool parseEventType(const char *text, EventType *out)
{
    char upper[NAME_BUF_SIZE];
    size_t len = strlen(text);

    if (len >= sizeof upper) return false;
    for (size_t i = 0; i <= len; i++)
        upper[i] = (char)toupper((unsigned char)text[i]);

    for (int i = 0; i < EVENT_COUNT; i++) {
        if (strcmp(upper, EVENT_NAMES[i]) == 0) {
            *out = (EventType)i;
            return true;
        }
    }
    return false;
}

/* ====================== 1. Event queue ====================== */

static void eventQueueInit(EventQueue *q)
{
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
}

static bool eventQueueIsEmpty(const EventQueue *q)
{
    return q->count == 0;
}

static bool eventQueueIsFull(const EventQueue *q)
{
    return q->count == QUEUE_CAPACITY;
}

/* Add at the tail. Returns false (and changes nothing) if the queue is full. */
static bool eventQueuePush(EventQueue *q, const Event *event)
{
    if (eventQueueIsFull(q))
        return false;

    q->items[q->tail] = *event;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;   /* wrap around */
    q->count++;
    return true;
}

/* Remove from the head. Returns false if the queue is empty. */
static bool eventQueuePop(EventQueue *q, Event *out)
{
    if (eventQueueIsEmpty(q))
        return false;

    *out = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;   /* wrap around */
    q->count--;
    return true;
}

/* Look at the n-th pending event (0 = oldest) WITHOUT removing it. */
static bool eventQueuePeek(const EventQueue *q, int offset, Event *out)
{
    if (offset < 0 || offset >= q->count)
        return false;

    *out = q->items[(q->head + offset) % QUEUE_CAPACITY];
    return true;
}

/* ====================== Callbacks ====================== */

static void paymentReceivedCallback(Event *event)
{
    if (event == NULL) return;
    printf("[PAYMENT] Received: Rs. ");
    printAmount(event->amount);
    printf("\n");
}

static void paymentSuccessCallback(Event *event)
{
    if (event == NULL) return;
    printf("[PAYMENT] Success: Rs. ");
    printAmount(event->amount);
    printf("\n");
    printf("[AUDIO] Playing: Payment Successful\n");
}

static void paymentFailedCallback(Event *event)
{
    if (event == NULL) return;
    printf("[PAYMENT] Failed: Rs. ");
    printAmount(event->amount);
    printf("\n");
    printf("[AUDIO] Playing: Payment Failed\n");
}

/* One callback serves both network events, so it checks the event type. */
static void networkCallback(Event *event)
{
    if (event == NULL) return;
    if (event->type == EVENT_NETWORK_CONNECTED)
        printf("[NETWORK] Connected\n");
    else if (event->type == EVENT_NETWORK_DISCONNECTED)
        printf("[NETWORK] Disconnected\n");
}

static void batteryCallback(Event *event)
{
    if (event == NULL) return;
    printf("[WARNING] Low Battery\n");
}

/* All callbacks the user is allowed to name in REGISTER. */
static const CallbackEntry CALLBACK_TABLE[] = {
    { "paymentReceivedCallback", paymentReceivedCallback },
    { "paymentSuccessCallback",  paymentSuccessCallback  },
    { "paymentFailedCallback",   paymentFailedCallback   },
    { "networkCallback",         networkCallback         },
    { "batteryCallback",         batteryCallback         }
};
#define CALLBACK_TABLE_SIZE (sizeof CALLBACK_TABLE / sizeof CALLBACK_TABLE[0])

/* ====================== 2. Callback registration ====================== */

static RegisterResult registerEventCallback(EventType type, const char *callbackName)
{
    const CallbackEntry *found = NULL;

    for (size_t i = 0; i < CALLBACK_TABLE_SIZE; i++) {
        if (strcmp(CALLBACK_TABLE[i].name, callbackName) == 0) {
            found = &CALLBACK_TABLE[i];
            break;
        }
    }
    if (found == NULL)
        return REGISTER_UNKNOWN_CALLBACK;

    /* The callback must be the one meant for this event. */
    if (strcmp(EXPECTED_CALLBACK[type], callbackName) != 0)
        return REGISTER_WRONG_EVENT;

    if (g_registry[type] != NULL)
        return REGISTER_DUPLICATE;

    g_registry[type] = found->function;
    return REGISTER_OK;
}

/* ====================== 3. Dispatcher ====================== */

/* Run the registered callback for one event (called at most once per event). */
static void eventDispatcher(Event *event)
{
    printf("[EVENT] %s\n", EVENT_NAMES[event->type]);

    EventCallback callback = g_registry[event->type];
    if (callback == NULL)
        printf("[WARNING] No callback registered for %s\n", EVENT_NAMES[event->type]);
    else
        callback(event);

    printf("\n");
}

/* Remove every queued event in FIFO order and dispatch it. */
static void processAllEvents(void)
{
    Event event;

    while (eventQueuePop(&g_queue, &event))
        eventDispatcher(&event);

    printf("Queue empty\n");
}

/* ====================== 4. Command handling ====================== */

static void cmdRegister(const char *eventText, const char *callbackName)
{
    EventType type;

    if (!parseEventType(eventText, &type)) {
        printf("Error: invalid event %s\n", eventText);
        return;
    }

    switch (registerEventCallback(type, callbackName)) {
        case REGISTER_OK:
            printf("Callback registered for %s\n", EVENT_NAMES[type]);
            break;
        case REGISTER_UNKNOWN_CALLBACK:
            printf("Error: invalid callback %s\n", callbackName);
            break;
        case REGISTER_WRONG_EVENT:
            printf("Error: %s cannot handle %s (use %s)\n",
                   callbackName, EVENT_NAMES[type], EXPECTED_CALLBACK[type]);
            break;
        case REGISTER_DUPLICATE:
            printf("Error: callback already registered for %s\n", EVENT_NAMES[type]);
            break;
    }
}

static void cmdPush(const char *eventText, const char *amountText)
{
    EventType type;
    double    amount;

    if (!parseEventType(eventText, &type)) {
        printf("Error: invalid event %s\n", eventText);
        return;
    }
    if (!parseAmount(amountText, &amount)) {
        printf("Error: amount must be a number, 0 or more\n");
        return;
    }

    Event event = { type, amount };
    if (eventQueuePush(&g_queue, &event))      /* PUSH only queues, never calls the callback */
        printf("Event %s queued\n", EVENT_NAMES[type]);
    else
        printf("Error: queue full, event %s rejected\n", EVENT_NAMES[type]);
}

static void cmdStatus(void)
{
    printf("Queue count: %d/%d\n", g_queue.count, QUEUE_CAPACITY);
}

static void cmdList(void)
{
    if (eventQueueIsEmpty(&g_queue)) {
        printf("Queue empty\n");
        return;
    }

    printf("Queued events:\n");
    for (int i = 0; i < g_queue.count; i++) {
        Event event;
        eventQueuePeek(&g_queue, i, &event);   /* peek: nothing is removed */
        printf("%s | ", EVENT_NAMES[event.type]);
        printAmount(event.amount);
        printf("\n");
    }
}

/* Handle one input line. Returns false when the program should stop (EXIT). */
static bool processCommand(char *line)
{
    char *tokens[4];
    int   n = 0;

    for (char *tok = strtok(line, " \t\r\n"); tok != NULL; tok = strtok(NULL, " \t\r\n")) {
        if (n < 4) tokens[n] = tok;
        n++;
    }
    if (n == 0) return true;                 /* blank line: ignore */

    for (char *p = tokens[0]; *p; p++)       /* commands are case-insensitive */
        *p = (char)toupper((unsigned char)*p);
    const char *cmd = tokens[0];

    if (strcmp(cmd, "REGISTER") == 0) {
        if (n != 3) printf("Error: usage: REGISTER <event> <callback>\n");
        else        cmdRegister(tokens[1], tokens[2]);
    } else if (strcmp(cmd, "PUSH") == 0) {
        if (n != 3) printf("Error: usage: PUSH <event> <amount>\n");
        else        cmdPush(tokens[1], tokens[2]);
    } else if (strcmp(cmd, "PROCESS") == 0) {
        if (n != 1) printf("Error: usage: PROCESS\n");
        else        processAllEvents();
    } else if (strcmp(cmd, "STATUS") == 0) {
        if (n != 1) printf("Error: usage: STATUS\n");
        else        cmdStatus();
    } else if (strcmp(cmd, "LIST") == 0) {
        if (n != 1) printf("Error: usage: LIST\n");
        else        cmdList();
    } else if (strcmp(cmd, "EXIT") == 0) {
        return false;
    } else {
        printf("Error: unknown command\n");
    }
    return true;
}

/* ====================== 5. main ====================== */

int main(void)
{
    char line[LINE_SIZE];

    eventQueueInit(&g_queue);

    while (fgets(line, sizeof line, stdin) != NULL) {
        /* Line longer than the buffer: discard the rest of it. */
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Error: line too long\n");
            continue;
        }
        if (!processCommand(line))
            break;
    }
    return 0;
}