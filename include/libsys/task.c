//
// simple multitasking support code
//
#include <setjmp.h>
#include <stdlib.h>
#include <stdarg.h>

#if 0
#define TASK_DEBUG(x) __builtin_printf x
#else
#define TASK_DEBUG(x)
#endif

#if !defined(__P2__)
#error Spin2 tasks are supported only on P2
#endif

#define NUM_TASKS 32
static volatile int cur_task = 0;
static unsigned active_tasks = 0;
static volatile unsigned halted_tasks = 0;

static struct task_state {
    jmp_buf j;
} __task[NUM_TASKS];

void __builtin_tasknext() {
    int t;
    TASK_DEBUG(("tasknext: active_tasks=0x%x\n", active_tasks));
    if (!active_tasks) {
        // all tasks have stopped
        TASK_DEBUG(("tasknext: no active tasks\n"));
        _Exit(0);
    }
    t = cur_task;
    TASK_DEBUG(("tasknext: save task %d\n", t));
    if (setjmp(&__task[t].j)) {
        TASK_DEBUG(("<- setjmp %d\n", cur_task));
        return;
    }
    for (;;) {
        unsigned mask;
        t = (t+1) & (NUM_TASKS-1);
        mask = 1<<t;
        if ( (active_tasks & mask) && !(halted_tasks & mask) ) {
            break;
        }
    }
    cur_task = t;
    TASK_DEBUG(("tasknext: longjmp -> %d\n", t));
    longjmp(&__task[t].j, 1);
}

void __builtin_taskstop(int task) {
    if (task == -1) task = cur_task;
    TASK_DEBUG(("*** taskstop: stop %d\n", task));
    active_tasks &= ~(1<<task);
    halted_tasks &= ~(1<<task);
    __builtin_tasknext();
}

void __builtin_taskhalt(int task) {
    if (task == -1) task = cur_task;
    TASK_DEBUG(("taskhalt: halt %d\n", task));
    halted_tasks |= (1<<task);
    __builtin_tasknext();
}

unsigned __builtin_gethalted() {
    return halted_tasks;
}

int __builtin_taskchk(int task) {
    unsigned mask = (1<<task);
    if (0 == (active_tasks & mask)) {
        return 0;
    }
    if ((halted_tasks & mask)) {
        return 2;
    }
    return 1;
}

void __builtin_taskcont(int task) {
    if (task == -1) task = cur_task;
    TASK_DEBUG(("taskcont: resume %d\n", task));
    halted_tasks &= ~(1<<task);
    __builtin_tasknext();
}

int __builtin_taskid(void) {
    return cur_task;
}

#define MAX_TASKC 8

int __builtin_taskstartv(int task, void *stackbot, void (*func)(), int argc, void **argv)
    __attribute__((task))
{
    unsigned char *stk;
    if (!active_tasks) {
        // initialize first task struct
        active_tasks = 1;
        cur_task = 0;
    }
    if (task < 0) {
        // find first free task
        unsigned i;
        for (i = 0; i < NUM_TASKS; i++) {
            if (!(active_tasks & (1<<i))) {
                break;
            }
        }
        TASK_DEBUG(("new task %d\n", i));
        task = i;
    }
    if (task >= NUM_TASKS) {
        return -1;
    }
    // save current state
    TASK_DEBUG(("save task %d\n", cur_task));
    if (setjmp(&__task[cur_task].j)) {
        // we came back here from the other task
        TASK_DEBUG(("taskstart: resuming %d\n", cur_task));
        return task;
    }
    // first time through; transform into the new
    // task
    __asm const {
        mov ptra, stackbot
    }
    TASK_DEBUG(("starting task: %d\n", task));
    cur_task = task;
    active_tasks |= (1<<task);
    switch (argc) {
    case 0: (*func)(); break;
    case 1: (*func)(argv[0]); break;
    case 2: (*func)(argv[0], argv[1]); break;
    case 3: (*func)(argv[0], argv[1], argv[2]); break;
    case 4: (*func)(argv[0], argv[1], argv[2], argv[3]); break;
    case 5: (*func)(argv[0], argv[1], argv[2], argv[4]); break;
    case 6: (*func)(argv[0], argv[1], argv[2], argv[4], argv[5]); break;
    case 7: (*func)(argv[0], argv[1], argv[2], argv[4], argv[5], argv[6]); break;
    case 8:
    default:
        (*func)(argv[0], argv[1], argv[2], argv[4], argv[5], argv[6], argv[7]);
        break;
    }

    TASK_DEBUG(("implicit taskstop\n"));
    // if we come back, stop
    __builtin_taskstop(-1);
    // we will never come back here
    return 0;
}

int __builtin_taskstartl(int task, void *stackbot, void (*func)(),
                         int argc, ...)
{
    static const char *argv[MAX_TASKC];
    va_list args;
    void *argx;

    TASK_DEBUG(("__builtin_taskstartl: argc=%d\n", argc));
    va_start(args, argc);

    for (int i = 0; i < argc; i++) {
        argx = va_arg(args, void *);
        argv[i] = argx;
        TASK_DEBUG(("arg[%d] = %x ('%s')\n", i, (unsigned)argx, (char *)argx));
    }
    __builtin_taskstartv(task, stackbot, func, argc, argv);
    va_end(args);
}


