#include "core/threadpool.h"

#include <thread>
#include <condition_variable>
#include <set>

class ThreadPoolPrivate {
public:
    ThreadPoolPrivate() :
            m_ActiveThreads(0) {
        PROFILE_FUNCTION();
    }

    Object *takeTask() {
        PROFILE_FUNCTION();
        if(!m_Tasks.empty()) {
            Object *object = m_Tasks.front();
            m_Tasks.pop();
            return object;
        }
        if(m_ActiveThreads > 0) {
            --m_ActiveThreads;
            m_Variable.notify_one();
        }
        return nullptr;
    }

    class APoolWorker {
    public:
        explicit APoolWorker    (ThreadPoolPrivate *pool);

        ~APoolWorker            ();

        void                    exec                        ();

        void                    run                         (Object *object);

        bool                    isFree                      ();

    protected:
        bool                    m_Enabled;

        thread                  m_Thread;

        condition_variable      m_Variable;

        Object                 *m_pTask;

        ThreadPoolPrivate      *m_pPool;
    };

public:
    condition_variable          m_Variable;

    mutex                       m_Mutex;

    set<APoolWorker *>          m_Workers;

    queue<Object *>             m_Tasks;

    int32_t                     m_ActiveThreads;
};

ThreadPoolPrivate::APoolWorker::APoolWorker(ThreadPoolPrivate *pool) :
        m_Enabled(true),
        m_pTask(nullptr),
        m_pPool(pool) {
    PROFILE_FUNCTION();
    m_Thread    = thread(&APoolWorker::exec, this);
}

ThreadPoolPrivate::APoolWorker::~APoolWorker() {
    PROFILE_FUNCTION();
    m_Enabled   = false;
    m_Variable.notify_one();
    m_Thread.join();
}

void ThreadPoolPrivate::APoolWorker::exec() {
    PROFILE_FUNCTION();
    while(m_Enabled) {
        unique_lock<mutex> locker(m_pPool->m_Mutex);
        m_Variable.wait(locker, [&]() { return (m_pTask != nullptr) || !m_Enabled; });

        if(m_pTask) {
            locker.unlock();
            m_pTask->processEvents();
            locker.lock();
        }
        m_pTask = m_pPool->takeTask();
        m_Variable.notify_one();
    }
}

void ThreadPoolPrivate::APoolWorker::run(Object *object) {
    PROFILE_FUNCTION();
    ++m_pPool->m_ActiveThreads;
    m_pTask = object;
    m_Variable.notify_one();
}

bool ThreadPoolPrivate::APoolWorker::isFree() {
    PROFILE_FUNCTION();
    return (m_pTask == nullptr);
}
/*!
    \class ThreadPool
    \brief The ThreadPool class manages a collection of threads.

    \since Next 1.0
    \inmodule Core
*/
ThreadPool::ThreadPool() :
        p_ptr(new ThreadPoolPrivate) {

    PROFILE_FUNCTION();
    setMaxThreads(optimalThreadCount());
}

ThreadPool::~ThreadPool() {
    PROFILE_FUNCTION();
    for(auto it : p_ptr->m_Workers) {
        delete it;
    }
    p_ptr->m_Workers.clear();
}
/*!
    Pushes an \a object to thread pool.
    In case of any free worker available executes task immediately.
*/
void ThreadPool::start(Object &object) {
    PROFILE_FUNCTION();
    unique_lock<mutex> locker(p_ptr->m_Mutex);
    for(auto it : p_ptr->m_Workers) {
        if(it->isFree()) {
            it->run(&object);
            return;
        }
    }
    p_ptr->m_Tasks.push(&object);
}
/*!
    Returns the max number of threads allocated to work.
*/
uint32_t ThreadPool::maxThreads() const {
    PROFILE_FUNCTION();
    return p_ptr->m_Workers.size();
}
/*!
    Sets the max \a number of threads allocated to work.
*/
void ThreadPool::setMaxThreads(uint32_t number) {
    PROFILE_FUNCTION();
    uint32_t current    = p_ptr->m_Workers.size();
    if(current < number) {
        for(uint32_t i = 0; i < number - current; i++) {
            ThreadPoolPrivate::APoolWorker *worker = new ThreadPoolPrivate::APoolWorker(p_ptr);
            Object *object     = p_ptr->takeTask();
            if(object) {
                worker->run(object);
            }
            p_ptr->m_Workers.insert(worker);
        }
    } else if(current > number) {
        for(uint32_t i = 0; i < current - number; i++) {
            auto it = p_ptr->m_Workers.end();
            --it;
            ThreadPoolPrivate::APoolWorker *worker = (*it);
            p_ptr->m_Workers.erase(it);
            delete worker;
        }
    }
}
/*!
    Waits up to \a msecs milliseconds for all threads to exit and removes all threads from the thread pool.
    Returns true if all threads were removed; otherwise it returns false.
    If \a msecs is -1 (the default), the timeout is ignored (waits for the last thread to exit).
*/
bool ThreadPool::waitForDone(int32_t msecs) {
    PROFILE_FUNCTION();
    unique_lock<mutex> locker(p_ptr->m_Mutex);
    if(msecs < 0) {
        while(true) {
            p_ptr->m_Variable.wait(locker, [&]() { return (p_ptr->m_Tasks.empty() && p_ptr->m_ActiveThreads == 0); });
            break;
        }
    }
    return (p_ptr->m_Tasks.empty() && p_ptr->m_ActiveThreads == 0);
}
/*!
    Returns the optimal thread count for the current system.
    This value is based on the number of CPU cores.
*/
uint32_t ThreadPool::optimalThreadCount() {
    PROFILE_FUNCTION();
    return thread::hardware_concurrency();
}
