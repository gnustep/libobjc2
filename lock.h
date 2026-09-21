/**
 * libobjc requires recursive mutexes.  These are delegated to the underlying
 * threading implementation.  This file contains a VERY thin wrapper over the
 * Windows and POSIX mutex APIs.
 */

#ifndef __LIBOBJC_LOCK_H_INCLUDED__
#define __LIBOBJC_LOCK_H_INCLUDED__
#ifdef _WIN32
#	include "safewindows.h"
typedef CRITICAL_SECTION mutex_t;
#	define INIT_LOCK(x) InitializeCriticalSection(&(x))
#	define LOCK(x) EnterCriticalSection(x)
#	define UNLOCK(x) LeaveCriticalSection(x)
#	define DESTROY_LOCK(x) DeleteCriticalSection(x)
// A slim reader/writer lock needs Windows Vista or later.  Its exclusive mode
// is a mutex, so a writer sees the same behaviour as a critical section.
typedef SRWLOCK rwlock_t;
#	define INIT_RWLOCK(x) InitializeSRWLock(&(x))
#	define RDLOCK(x) AcquireSRWLockShared(x)
#	define RDUNLOCK(x) ReleaseSRWLockShared(x)
#	define WRLOCK(x) AcquireSRWLockExclusive(x)
#	define WRUNLOCK(x) ReleaseSRWLockExclusive(x)
#	define DESTROY_RWLOCK(x) (void)(x)
#else

#	include <pthread.h>

typedef pthread_mutex_t mutex_t;
// If this pthread implementation has a static initializer for recursive
// mutexes, use that, otherwise fall back to the portable version
#	ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#		define INIT_LOCK(x) x = (pthread_mutex_t)PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#	elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
#		define INIT_LOCK(x) x = (pthread_mutex_t)PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#	else
#		define INIT_LOCK(x) init_recursive_mutex(&(x))

static inline void init_recursive_mutex(pthread_mutex_t *x)
{
	pthread_mutexattr_t recursiveAttributes;
	pthread_mutexattr_init(&recursiveAttributes);
	pthread_mutexattr_settype(&recursiveAttributes, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(x, &recursiveAttributes);
	pthread_mutexattr_destroy(&recursiveAttributes);
}
#	endif

#	define LOCK(x) pthread_mutex_lock(x)
#	define UNLOCK(x) pthread_mutex_unlock(x)
#	define DESTROY_LOCK(x) pthread_mutex_destroy(x)

typedef pthread_rwlock_t rwlock_t;
#	define INIT_RWLOCK(x) pthread_rwlock_init(&(x), NULL)
#	define RDLOCK(x) pthread_rwlock_rdlock(x)
#	define RDUNLOCK(x) pthread_rwlock_unlock(x)
#	define WRLOCK(x) pthread_rwlock_wrlock(x)
#	define WRUNLOCK(x) pthread_rwlock_unlock(x)
#	define DESTROY_RWLOCK(x) pthread_rwlock_destroy(x)
#endif

__attribute__((unused)) static void objc_release_lock(void *x)
{
	mutex_t *lock = *(mutex_t**)x;
	UNLOCK(lock);
}
/**
 * Concatenate strings during macro expansion.
 */
#define LOCK_HOLDERN_NAME_CAT(x, y) x ## y
/**
 * Concatenate string with unique variable during macro expansion.
 */
#define LOCK_HOLDER_NAME_COUNTER(x, y) LOCK_HOLDERN_NAME_CAT(x, y)
/**
 * Create a unique name for a lock holder variable
 */
#define LOCK_HOLDER_NAME(x) LOCK_HOLDER_NAME_COUNTER(x, __COUNTER__)

/**
 * Acquires the lock and automatically releases it at the end of the current
 * scope.
 */
#define LOCK_FOR_SCOPE(lock) \
	__attribute__((cleanup(objc_release_lock)))\
	__attribute__((unused)) mutex_t *LOCK_HOLDER_NAME(lock_pointer) = lock;\
	LOCK(lock)

/**
 * The global runtime mutex.
 */
extern 
#ifdef __cplusplus
"C"
#endif
mutex_t runtime_mutex;

#define LOCK_RUNTIME() LOCK(&runtime_mutex)
#define UNLOCK_RUNTIME() UNLOCK(&runtime_mutex)
#define LOCK_RUNTIME_FOR_SCOPE() LOCK_FOR_SCOPE(&runtime_mutex)

#ifdef __cplusplus
/**
 * C++ wrapper around our mutex, for use with std::lock_guard and friends.
 */
class RecursiveMutex
{
	/// The underlying mutex
	mutex_t mutex;

	public:
	/**
	 * Explicit initialisation of the underlying lock, so that this can be a
	 * global.
	 */
	void init()
	{
		INIT_LOCK(mutex);
	}

	/// Acquire the lock.
	void lock()
	{
		LOCK(&mutex);
	}

	/// Release the lock.
	void unlock()
	{
		UNLOCK(&mutex);
	}
};

/**
 * A lock that many readers may hold at once, or one writer exclusively.  It is
 * not recursive: a thread that holds it must not acquire it again.
 */
class ReadWriteLock
{
	/// The underlying lock
	rwlock_t rwlock;

	public:
	/**
	 * Explicit initialisation of the underlying lock, so that this can be a
	 * global.
	 */
	void init()
	{
		INIT_RWLOCK(rwlock);
	}

	/// Acquire the lock for writing.
	void lock()
	{
		WRLOCK(&rwlock);
	}

	/// Release the lock after writing.
	void unlock()
	{
		WRUNLOCK(&rwlock);
	}

	/// Acquire the lock for reading.
	void lock_shared()
	{
		RDLOCK(&rwlock);
	}

	/// Release the lock after reading.
	void unlock_shared()
	{
		RDUNLOCK(&rwlock);
	}
};
#endif

#endif // __LIBOBJC_LOCK_H_INCLUDED__
