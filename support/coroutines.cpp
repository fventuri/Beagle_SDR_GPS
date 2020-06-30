#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "valgrind.h"
#include "misc.h"
#include "str.h"
#include "coroutines.h"
#include "debug.h"
#include "peri.h"

#include <pthread.h>
#include <sched.h>


// global variables
u4_t task_medium_priority = TASK_MED_PRI_NEW;
struct Task{
    int id;

    void* user_parameter;
    void* wake_param;

    const char* name;
    funcP_t entry;
    u4_t flags;
    int f_arg;
    int priority;

    bool killed;
    bool sleeping;

    // the object to support sleep/wakeup
    pthread_t pthread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

bool itask_run;
struct Task Tasks[MAX_TASKS];

static int itask_tid = 0;
static Task* itask = nullptr;
static __thread struct Task *current;

void TaskInit()
{
    Task *current_task = &Tasks[0];

    current_task->id = 0;
    current_task->entry = (funcP_t)1;
    current_task->name = "main thread";

    // create the mutex and cond
    pthread_cond_init(&current_task->cond, NULL);
    pthread_mutex_init(&current_task->mutex, NULL);

    current = current_task;
}

void TaskInitCfg()
{
//TODO: figure out priority of threads
}

void TaskCollect()
{
}

void TaskForkChild()
{
    printf("We don't support fork\n");
}

bool TaskIsChild()
{
    return false;
}

// Should be a nop
void TaskPollForInterrupt(ipoll_from_e from)
{
    if (itask == nullptr) {
        return;
    }

    if (GPIO_READ_BIT(SND_INTR) && itask->sleeping) {
        TaskWakeup(itask_tid, TWF_NONE, 0);
    }
}

void TaskCheckStacks(bool report)
{
}

const char* Task_s(int id)
{
    return Tasks[id].name;
}

// TODO: This is ugly, we should not do this
void TaskSleepID(int id, int usec)
{
    if (current->id == id)
    {
        _TaskSleep("TaskSleepId", usec, NULL);
    }
    else
    {
        panic("Stop sleep other task.");
    }
}

void _NextTask(const char *s, u4_t param, u_int64_t pc)
{
    if (current->killed)
    {
        current->entry = nullptr;
        pthread_cond_destroy(&current->cond);
        pthread_mutex_destroy(&current->mutex);
        pthread_exit(NULL);
        return;
    }

    TaskPollForInterrupt(CALLED_WITHIN_NEXTTASK);
}

void TaskRemove(int id)
{
    Tasks[id].killed = true;
    if (id == current->id)
    {
        _NextTask(NULL, 0, 0);
    }
}

// wait on the current task's sleep condition variable
void *_TaskSleep(const char *reason, int usec, u4_t *wakeup_test)
{
    int ret;
    struct timespec max_wait = {0, 0};
    if (usec > 0)
    {
        ret = clock_gettime(CLOCK_REALTIME, &max_wait);
        int sec = usec / 1000 / 1000;
        max_wait.tv_sec += sec;
        max_wait.tv_nsec += (usec - sec * 1000 * 1000) * 1000;
    }

    ret = pthread_mutex_lock(&current->mutex);
    current->sleeping = true;
    if (usec > 0)
    {
        ret = pthread_cond_timedwait(&current->cond, &current->mutex,
            &max_wait);
    }
    else
    {
        ret = pthread_cond_wait(&current->cond, &current->mutex);
    }

    current->sleeping = false;
    ret = pthread_mutex_unlock(&current->mutex);	

    return current->wake_param;
}

// Wakeup the sepecific task via signal the condition variable
void TaskWakeup(int id, u4_t flags, void *wake_param)
{
    pthread_mutex_lock(&Tasks[id].mutex);
    Tasks[id].wake_param = wake_param;
    pthread_cond_signal(&Tasks[id].cond);
    pthread_mutex_unlock(&Tasks[id].mutex);	
}

// thread entry for every thread
static void* ThreadEntry(void* parameter)
{
    // setup the thread local variable to point to the thread structure
    struct Task* current_task = (struct Task*)parameter;

    // create the mutex and cond
    pthread_cond_init(&current_task->cond, NULL);
    pthread_mutex_init(&current_task->mutex, NULL);

    current = current_task;

    // call user function
    current_task->entry(current_task->user_parameter);

    pthread_cond_destroy(&current->cond);
    pthread_mutex_destroy(&current->mutex);
    pthread_exit(0);
}

// create the task and invoke the entry function
int _CreateTask(funcP_t entry, const char *name, void *param, int priority, u4_t flags, int f_arg)
{
    struct Task *current_task = nullptr;
    for(auto i = 1; i < MAX_TASKS; i++)
    {
        if (Tasks[i].entry == nullptr)
        {
            current_task = &Tasks[i];
	    memset(current_task, 0, sizeof(Task));
	    current_task->id = i;
            break;
        }
    }

    if (current_task == nullptr)
    {
        // out of tasks
        dump();
        panic("create_task: no tasks available");
    }

    // Initialize the structure
    current_task->entry = entry;
    current_task->name = name;
    current_task->user_parameter = param;
    current_task->priority = priority;
    current_task->flags = flags;
    current_task->f_arg = f_arg;

    // initialize attributes for thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // set priority
    // pthread_attr_setschedparam(&attr, );


    pthread_create(&current_task->pthread, &attr, ThreadEntry, current_task);

    pthread_setname_np(current_task->pthread, name);

    if (flags & CTF_POLL_INTR) {
        assert(!itask);
        itask = current_task;
        itask_tid = current_task->id;
    }

    return current_task->id;
}

// TODO
void TaskDump(u4_t flags)
{
}

//TODO
int TaskStat(u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func, int s2_val, const char *s2_units)
{
    return 0;
}

u4_t TaskPriority(int priority)
{
    if (!current) return 0;
    if (priority == -1) return current->priority;
    return current->priority;
}

u4_t TaskID()
{
    if (!current) return 0;
    return current->id;
}

const char *_TaskName(const char *name, bool free_name)
{
    struct Task *ct = current;
    
    if (!ct) return "main";
    if (name != NULL) {
        if (ct->flags & CTF_TNAME_FREE) {
            free((void *) ct->name);
            ct->flags &= ~CTF_TNAME_FREE;
        }
        ct->name = name;
        if (free_name) ct->flags |= CTF_TNAME_FREE;
    }
    return ct->name;
}

void TaskMinRun(u4_t minrun_us)
{
    struct Task *t = current;

    //t->minrun = minrun_us;
}

u4_t TaskFlags()
{
    struct Task *t = current;

    return t->flags;
}

void TaskSetFlags(u4_t flags)
{
    struct Task *t = current;

    t->flags = flags;
}

void *TaskGetUserParam()
{
    return current->user_parameter;
}

void TaskSetUserParam(void *param)
{
    current->user_parameter = param;
}

/*
 * ------------------------------------------------------------
 *  Lock related functions
 */
#define MAX_LOCK_N 256
static int n_lock_list = 0;
static lock_t *locks[MAX_LOCK_N];

void _lock_init(lock_t *lock, const char *name)
{
    memset(lock, 0, sizeof(*lock));
    lock->name = name;
    pthread_mutex_init(&lock->mutex, NULL);
}

void lock_register(lock_t *lock)
{
    int lock_index = __atomic_fetch_add(&n_lock_list, 1, __ATOMIC_SEQ_CST);
    locks[lock_index] = lock;
}

void lock_dump()
{
    //TODO
}

bool lock_check()
{
    // TODO
    return false;
}

void lock_enter(lock_t *lock)
{
    pthread_mutex_lock(&lock->mutex);
}

void lock_leave(lock_t *lock)
{
    pthread_mutex_unlock(&lock->mutex);
}

