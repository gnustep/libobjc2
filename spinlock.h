#include "visibility.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>


// Not all supported targets implement the wait / notify instructions on
// atomics.  Provide simple spinning fallback for ones that don't.
template<typename T>
concept Waitable = requires(T t)
{
	t.notify_all();
};

/**
 * Lightweight spinlock that falls back to using the operating system's futex
 * abstraction if one exists.
 */
class ThinLock
{
	// Underlying type for the lock word.  Should be the type used for futexes
	// and similar.
	using LockWordUnderlyingType = uint32_t;
	// A lock word, as a 3-state state machine.
	enum LockState : LockWordUnderlyingType
	{
		Unlocked = 0,
		Locked = 1,
		LockedWithWaiters = 3
	};
	// The lock word
	std::atomic<LockState> lockWord;

	// Call notify if it exists
	template<typename T>
	static void notify(T &atomic) requires (Waitable<T>)
	{
		atomic.notify_all();
	}

	// Simply ignore notify if we don't have one.
	template<typename T>
	static void notify(T &atomic) requires (!Waitable<T>)
	{
	}

	// Wait if we have a wait method.
	template<typename T>
	static void wait(T &atomic, LockState expected) requires (Waitable<T>)
	{
		atomic.wait(expected);
	}

	// Short sleep if we don't.
	template<typename T>
	static void wait(T &atomic, LockState expected) requires (!Waitable<T>)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1ms);
	}

	public:
	// Acquire the lock
	void lock()
	{
		while (true)
		{
			auto old     = LockState::Unlocked;
			// CAS in the locked state.
			if (lockWord.compare_exchange_strong(old, LockState::Locked))
			{
				return;
			}
			// If the CAS failed add the waiters flag.
			if (old != LockState::LockedWithWaiters)
			{
				if (!lockWord.compare_exchange_strong(old, LockState::LockedWithWaiters))
				{
					// If the CAS failed, this means that we lost the race, retry.
					continue;
				}
			}
			wait(lockWord, LockState::LockedWithWaiters);
		}
	}

	// Release the lock
	void unlock()
	{
		auto old = lockWord.exchange(LockState::Unlocked);
		if (old == LockState::LockedWithWaiters)
		{
			notify(lockWord);
		}
	}

	// Stable ordering of locks.
	// This should be operator<=>, but we support targets with no <compare> 
	auto operator==(ThinLock &other)
	{
		return this == &other;
	}

	auto operator<(ThinLock &other)
	{
		return this < &other;
	}
};

/**
 * Deadlock-free lock guard.  Acquires the two locks in order defined by their
 * sort order.  If the two locks are the same, does not acquire the second.
 */
class DoubleLockGuard
{
	// The first lock
	ThinLock *lock1;
	// The second lock
	ThinLock *lock2;
	public:
	DoubleLockGuard(ThinLock *lock1, ThinLock *lock2) : lock1(lock1), lock2(lock2)
	{
		if (lock2 < lock1)
		{
			std::swap(lock1, lock2);
		}
		lock1->lock();
		if (lock1 != lock2)
		{
			lock2->lock();
		}
	}
	~DoubleLockGuard()
	{
		assert(lock2 >= lock1);
		lock1->unlock();
		if (lock1 != lock2)
		{
			lock2->unlock();
		}
	}
};

/**
 * Number of spinlocks.  This allocates one page with a 32-bit lock.
 */
#define spinlock_count (1<<10)
static const int spinlock_mask = spinlock_count - 1;
/**
 * Integers used as spinlocks for atomic property access.
 */
PRIVATE inline ThinLock spinlocks[spinlock_count];

/**
 * Get a spin lock from a pointer.  We want to prevent lock contention between
 * properties in the same object - if someone is stupid enough to be using
 * atomic property access, they are probably stupid enough to do it for
 * multiple properties in the same object.  We also want to try to avoid
 * contention between the same property in different objects, so we can't just
 * use the ivar offset.
 */
static inline ThinLock *lock_for_pointer(const void *ptr)
{
	intptr_t hash = (intptr_t)ptr;
	// Most properties will be pointers, so disregard the lowest few bits
	hash >>= sizeof(void*) == 4 ? 2 : 8;
	intptr_t low = hash & spinlock_mask;
	hash >>= 16;
	hash |= low;
	return spinlocks + (hash & spinlock_mask);
}

/**
 * Returns a lock guard for the lock associated with a single address.
 */
inline auto acquire_locks_for_pointers(const void *ptr)
{
	return std::lock_guard<ThinLock>{*lock_for_pointer(ptr)};
}

/**
 * Returns a lock guard for locks associated with two addresses.
 */
inline auto acquire_locks_for_pointers(const void *ptr, const void *ptr2)
{
	return DoubleLockGuard(lock_for_pointer(ptr), lock_for_pointer(ptr2));
}
