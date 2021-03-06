#pragma once

#ifndef _WIN32

#include "../atomic.h"
#include "../closure.h"

#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <memory>

class Mutex {
   public:
    Mutex() {
        assert(0 == pthread_mutex_init(&_mutex, 0));
    }

    ~Mutex() {
        assert(0 == pthread_mutex_destroy(&_mutex));
    }

    void lock() {
        assert(0 == pthread_mutex_lock(&_mutex));
    }

    void unlock() {
        assert(0 == pthread_mutex_unlock(&_mutex));
    }

    bool try_lock() { return pthread_mutex_trylock(&_mutex) == 0; }

    pthread_mutex_t* mutex() { return &_mutex; }

   private:
    pthread_mutex_t _mutex;
    DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class RwMutex {
   public:
    RwMutex() {
        assert(0 == pthread_rwlock_init(&_mutex, nullptr));
    }

    ~RwMutex() {
        assert(0 == pthread_rwlock_destroy(&_mutex));
    }

    void rlock() {
        assert(0 == pthread_rwlock_rdlock(&_mutex));
    }

    void wlock() {
        assert(0 == pthread_rwlock_wrlock(&_mutex));
    }

    bool try_rlock() { return pthread_rwlock_tryrdlock(&_mutex) == 0; }

    bool try_wlock() { return pthread_rwlock_trywrlock(&_mutex) == 0; }

    bool try_rlock_timeout(struct timespec& t) {
        return pthread_rwlock_timedrdlock(&_mutex, &t) == 0;
    }

    bool try_wlock_timeout(struct timespec& t) {
        return pthread_rwlock_timedwrlock(&_mutex, &t) == 0;
    }

    void unlock() {
        assert(0 == pthread_rwlock_unlock(&_mutex));
    }

    pthread_rwlock_t* mutex() { return &_mutex; }

   private:
    pthread_rwlock_t _mutex;
    DISALLOW_COPY_AND_ASSIGN(RwMutex);
};

class MutexGuard {
   public:
    explicit MutexGuard(Mutex& lock) : _lock(lock) { _lock.lock(); }

    explicit MutexGuard(Mutex* lock) : _lock(*lock) { _lock.lock(); }

    void lock() { _lock.lock(); }
    void unlock() { _lock.unlock(); }

    ~MutexGuard() { _lock.unlock(); }

   private:
    Mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};

class RMutexGuard {
   public:
    explicit RMutexGuard(RwMutex& lock) : _lock(lock) { _lock.rlock(); }
    explicit RMutexGuard(RwMutex* lock) : _lock(*lock) { _lock.rlock(); }

    ~RMutexGuard() { _lock.unlock(); }

    void lock() { _lock.rlock(); }
    void unlock() { _lock.unlock(); }

   private:
    RwMutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(RMutexGuard);
};

class WMutexGuard {
   public:
    explicit WMutexGuard(RwMutex& lock) : _lock(lock) { _lock.wlock(); }
    explicit WMutexGuard(RwMutex* lock) : _lock(*lock) { _lock.wlock(); }

    ~WMutexGuard() { _lock.unlock(); }

    void lock() { _lock.wlock(); }
    void unlock() { _lock.unlock(); }

   private:
    RwMutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(WMutexGuard);
};

class SyncEvent {
   public:
    explicit SyncEvent(bool manual_reset = false, bool signaled = false);

    ~SyncEvent() { pthread_cond_destroy(&_cond); }

    void signal() {
        MutexGuard g(_mutex);
        if (!_signaled) {
            _signaled = true;
            pthread_cond_broadcast(&_cond);
        }
    }

    void reset() {
        MutexGuard g(_mutex);
        _signaled = false;
    }

    void wait();

    // return false if timeout
    bool wait(unsigned int ms);

   private:
    pthread_cond_t _cond;
    Mutex _mutex;

    int _consumer;
    const bool _manual_reset;
    bool _signaled;

    DISALLOW_COPY_AND_ASSIGN(SyncEvent);
};

// starts a thread:
//   Thread x(f);                        // void f();
//   Thread x(f, p);                     // void f(void*);  void* p;
//   Thread x(&T::f, &t);                // void T::f();  T t;
//   Thread x(std::bind(f, 7));          // void f(int v);
//   Thread x(std::bind(&T::f, &t, 7));  // void T::f(int v);  T t;
//
// run independently from thread object:
//   Thread(f).detach();                 // void f();
class Thread {
   public:
    // @cb is not saved in this thread object, but passed directly to the
    // thread function, so it can run independently from the thread object.
    explicit Thread(Closure* cb) : _id(0) {
        assert(0 == pthread_create(&_id, 0, &Thread::_Run, cb));
    }

    explicit Thread(void (*f)()) : Thread(new_callback(f)) {}

    Thread(void (*f)(void*), void* p) : Thread(new_callback(f, p)) {}

    template <typename T>
    Thread(void (T::*f)(), T* p) : Thread(new_callback(f, p)) {}

    explicit Thread(std::function<void()>&& f)
        : Thread(new_callback(std::move(f))) {}

    explicit Thread(const std::function<void()>& f) : Thread(new_callback(f)) {}

    ~Thread() { this->join(); }

    // wait until the thread function terminates
    void join() {
        pthread_t id = atomic_swap(&_id, (pthread_t)0);
        if (id != 0) pthread_join(id, 0);
    }

    void detach() {
        pthread_t id = atomic_swap(&_id, (pthread_t)0);
        if (id != 0) pthread_detach(id);
    }

   private:
    pthread_t _id;
    DISALLOW_COPY_AND_ASSIGN(Thread);

    static void* _Run(void* p) {
        Closure* cb = (Closure*)p;
        if (cb) cb->run();
        return 0;
    }
};

namespace xx {
unsigned int gettid();  // get current thread id
}  // namespace xx

// using current_thread_id here as glibc 2.30 already has a gettid
inline unsigned int current_thread_id() {
    static __thread unsigned int id = 0;
    if (id != 0) return id;
    return id = xx::gettid();
}

// thread_ptr is based on TLS. Each thread sets and holds its own pointer.
// It is easy to use, just like the std::unique_ptr.
//   thread_ptr<T> pt;
//   if (!pt) pt.reset(new T);
template <typename T>
class thread_ptr {
   public:
    thread_ptr() {
        int r = pthread_key_create(&_key, 0);
        assert(r == 0);
    }

    ~thread_ptr() {
        int r = pthread_key_delete(_key);
        assert(r == 0);

        for (auto it = _objs.begin(); it != _objs.end(); ++it) {
            delete it->second;
        }
    }

    T* get() const { return (T*)pthread_getspecific(_key); }

    void reset(T* p = 0) {
        T* obj = this->get();
        if (obj == p) return;

        delete obj;
        pthread_setspecific(_key, p);

        {
            MutexGuard g(_mtx);
            _objs[current_thread_id()] = p;
        }
    }

    void operator=(T* p) { this->reset(p); }

    T* release() {
        T* obj = this->get();
        pthread_setspecific(_key, 0);
        {
            MutexGuard g(_mtx);
            _objs[current_thread_id()] = 0;
        }
        return obj;
    }

    T* operator->() const {
        T* obj = this->get();
        assert(obj);
        return obj;
    }

    T& operator*() const {
        T* obj = this->get();
        assert(obj);
        return *obj;
    }

    bool operator==(T* p) const { return this->get() == p; }

    bool operator!=(T* p) const { return this->get() != p; }

    bool operator!() const { return this->get() == 0; }

   private:
    pthread_key_t _key;

    Mutex _mtx;
    std::unordered_map<unsigned int, T*> _objs;

    DISALLOW_COPY_AND_ASSIGN(thread_ptr);
};

#endif
