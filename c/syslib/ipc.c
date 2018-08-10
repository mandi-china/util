//
// Created by hujianzhe
//

#include "ipc.h"
#include "assert.h"
#include <errno.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

/* signal */
sighandler_t signalRegHandler(int signo, sighandler_t func) {
#if defined(_WIN32) || defined(_WIN64)
	return signal(signo, func);
#else
	struct sigaction act, oact;
	act.sa_handler = func;
	sigfillset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	return sigaction(signo, &act, &oact) < 0 ? SIG_ERR : oact.sa_handler;
#endif
}

/* pipe */
BOOL pipeCreate(FD_t* r, FD_t* w) {
#if defined(_WIN32) || defined(_WIN64)
	/* note: IOCP can't use anonymous pipes(without an overlapped flag) */
	SECURITY_ATTRIBUTES sa = {0};
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	return CreatePipe((PHANDLE)r, (PHANDLE)w, &sa, 0);
#else
	int fd[2];
	if (pipe(fd)) {
		return FALSE;
	}
	*r = fd[0];
	*w = fd[1];
	return TRUE;
#endif
}

BOOL pipeNonBlock(FD_t pipefd, BOOL bool_val) {
#if defined(_WIN32) || defined(_WIN64)
	DWORD mode = bool_val ? PIPE_NOWAIT : PIPE_WAIT;
	return SetNamedPipeHandleState((HANDLE)pipefd, &mode, NULL, NULL);
#else
	return ioctl(pipefd, FIONBIO, &bool_val) == 0;
#endif
}

int pipeReadableBytes(FD_t r) {
#if defined(_WIN32) || defined(_WIN64)
	DWORD bytes;
	return PeekNamedPipe((HANDLE)r, NULL, 0, NULL, &bytes, NULL) ? bytes : -1;
#else
	int bytes;
	return ioctl(r, FIONREAD, &bytes) ? -1 : bytes;
#endif
}

/* critical section */
BOOL criticalsectionCreate(CriticalSection_t* cs) {
#if defined(_WIN32) || defined(_WIN64)
	__try {
		InitializeCriticalSection(cs);
	}
	__except (STATUS_NO_MEMORY) {
		return FALSE;
	}
	return TRUE;
#else
	int res;
	int attr_ok = 0;
	pthread_mutexattr_t attr;
	do {
		res = pthread_mutexattr_init(&attr);
		if (res)
			break;
		attr_ok = 1;
		res = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (res)
			break;
		res = pthread_mutex_init(cs, &attr);
		if (res)
			break;
	} while (0);
	if (attr_ok) {
		assertTRUE(0 == pthread_mutexattr_destroy(&attr));
	}
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

BOOL criticalsectionTryEnter(CriticalSection_t* cs) {
#if defined(_WIN32) || defined(_WIN64)
	return TryEnterCriticalSection(cs);
#else
	int res = pthread_mutex_trylock(cs);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

void criticalsectionEnter(CriticalSection_t* cs) {
#if defined(_WIN32) || defined(_WIN64)
	EnterCriticalSection(cs);
#else
	assertTRUE(pthread_mutex_lock(cs) == 0);
#endif
}

void criticalsectionLeave(CriticalSection_t* cs) {
#if defined(_WIN32) || defined(_WIN64)
	LeaveCriticalSection(cs);
#else
	assertTRUE(pthread_mutex_unlock(cs) == 0);
#endif
}

void criticalsectionClose(CriticalSection_t* cs) {
#if defined(_WIN32) || defined(_WIN64)
	DeleteCriticalSection(cs);
#else
	assertTRUE(pthread_mutex_destroy(cs) == 0);
#endif
}

/* condition */
BOOL conditionvariableCreate(ConditionVariable_t* condition) {
#if defined(_WIN32) || defined(_WIN64)
	InitializeConditionVariable(condition);
	return TRUE;
#else
	int res = pthread_cond_init(condition, NULL);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

BOOL conditionvariableWait(ConditionVariable_t* condition, CriticalSection_t* cs, int msec) {
#if defined(_WIN32) || defined(_WIN64)
	return SleepConditionVariableCS(condition, cs, msec);
#else
	int res;
	do {
		if (msec == INFTIM) {
			res = pthread_cond_wait(condition, cs);
		}
		else {
			struct timeval utc = {0};
			struct timezone zone = {0};
			struct timespec time_val = {0};
			if (gettimeofday(&utc, &zone)) {
				res = errno;
				break;
			}
			time_val.tv_sec = utc.tv_sec + msec / 1000;
			msec %= 1000;
			time_val.tv_nsec = utc.tv_usec * 1000 + msec * 1000000;
			time_val.tv_sec += time_val.tv_nsec / 1000000000;
			time_val.tv_nsec %= 1000000000;

			res = pthread_cond_timedwait(condition, cs, &time_val);
			if (res) {
				break;
			}
		}
	} while (0);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

void conditionvariableSignal(ConditionVariable_t* condition) {
#if defined(_WIN32) || defined(_WIN64)
	WakeConditionVariable(condition);
#else
	assertTRUE(pthread_cond_signal(condition) == 0);
#endif
}

void conditionvariableBroadcast(ConditionVariable_t* condition) {
#if defined(_WIN32) || defined(_WIN64)
	WakeAllConditionVariable(condition);
#else
	assertTRUE(pthread_cond_broadcast(condition) == 0);
#endif
}

void conditionvariableClose(ConditionVariable_t* condition) {
#if defined(_WIN32) || defined(_WIN64)
#else
	assertTRUE(pthread_cond_destroy(condition) == 0);
#endif
}

/* mutex */
BOOL mutexCreate(Mutex_t* mutex) {
#if defined(_WIN32) || defined(_WIN64)
	/* windows mutex will auto release after it's owner thread exit. */
	/* then...if another thread call WaitForSingleObject for that mutex,it will return WAIT_ABANDONED */
	HANDLE res = CreateEvent(NULL, FALSE, TRUE, NULL);/* so,I use event. */
	if (res) {
		*mutex = res;
		return TRUE;
	}
	return FALSE;
#else
	int res = pthread_mutex_init(mutex, NULL);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

BOOL mutexTryLock(Mutex_t* mutex) {
#if defined(_WIN32) || defined(_WIN64)
	return WaitForSingleObject(*mutex, 0) == WAIT_OBJECT_0;
#else
	int res = pthread_mutex_trylock(mutex);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

void mutexLock(Mutex_t* mutex) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0);
#else
	assertTRUE(pthread_mutex_lock(mutex) == 0);
#endif
}

void mutexUnlock(Mutex_t* mutex) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(SetEvent(*mutex));
#else
	assertTRUE(pthread_mutex_unlock(mutex) == 0);
#endif
}

void mutexClose(Mutex_t* mutex) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(CloseHandle(*mutex));
#else
	assertTRUE(pthread_mutex_destroy(mutex) == 0);
#endif
}

/* read/write lock */
BOOL rwlockCreate(RWLock_t* rwlock) {
#if defined(_WIN32) || defined(_WIN64)
	/* I don't use SRW Locks because SRW Locks are neither fair nor FIFO. */
	/*InitializeSRWLock(rwlock);*/
	int read_ev_ok = 0, write_ev_ok = 0;
	do {
		rwlock->__read_ev = CreateEvent(NULL, TRUE, TRUE, NULL);/* manual reset event */
		if (!rwlock->__read_ev) {
			break;
		}
		read_ev_ok = 1;
		rwlock->__write_ev = CreateEvent(NULL, FALSE, TRUE, NULL);/* auto reset event */
		if (!rwlock->__write_ev) {
			break;
		}
		write_ev_ok = 1;
		rwlock->__wait_ev = CreateEvent(NULL, FALSE, TRUE, NULL);/* auto reset event */
		if (!rwlock->__wait_ev) {
			break;
		}
		rwlock->__exclusive_lock = FALSE;
		rwlock->__read_cnt = 0;
		return TRUE;
	} while (0);
	if (read_ev_ok) {
		assertTRUE(CloseHandle(rwlock->__read_ev));
	}
	if (write_ev_ok) {
		assertTRUE(CloseHandle(rwlock->__write_ev));
	}
	return FALSE;
#else
	int res;
	#ifdef  __linux__
	int attr_ok = 0;
	pthread_rwlockattr_t attr;
	do {
		res = pthread_rwlockattr_init(&attr);
		if (res) {
			break;
		}
		attr_ok = 1;
		/* write thread avoid starving */
		res = pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
		if (res) {
			break;
		}
		res = pthread_rwlock_init(rwlock, &attr);
		if (res) {
			break;
		}
	} while (0);
	if (attr_ok) {
		assertTRUE(0 == pthread_rwlockattr_destroy(&attr));
	}
	#else
	res = pthread_rwlock_init(rwlock, NULL);
	#endif
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

void rwlockLockRead(RWLock_t* rwlock) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(WaitForSingleObject(rwlock->__read_ev, INFINITE) == WAIT_OBJECT_0);
	assertTRUE(WaitForSingleObject(rwlock->__wait_ev, INFINITE) == WAIT_OBJECT_0);
	if (0 == rwlock->__read_cnt++) {
		assertTRUE(WaitForSingleObject(rwlock->__write_ev, INFINITE) == WAIT_OBJECT_0);
	}
	assertTRUE(SetEvent(rwlock->__wait_ev));
#else
	assertTRUE(pthread_rwlock_rdlock(rwlock) == 0);
#endif
}

void rwlockLockWrite(RWLock_t* rwlock) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(ResetEvent(rwlock->__read_ev));
	assertTRUE(WaitForSingleObject(rwlock->__write_ev, INFINITE) == WAIT_OBJECT_0);
	rwlock->__exclusive_lock = TRUE;
#else
	assertTRUE(pthread_rwlock_wrlock(rwlock) == 0);
#endif
}

void rwlockUnlock(RWLock_t* rwlock) {
#if defined(_WIN32) || defined(_WIN64)
	if (rwlock->__exclusive_lock) {
		/*ReleaseSRWLockExclusive(rwlock);*/
		rwlock->__exclusive_lock = FALSE;
		assertTRUE(SetEvent(rwlock->__write_ev));
		assertTRUE(SetEvent(rwlock->__read_ev));
	}
	else {
		/*ReleaseSRWLockShared(rwlock);*/
		assertTRUE(WaitForSingleObject(rwlock->__wait_ev, INFINITE) == WAIT_OBJECT_0);
		if (0 == --rwlock->__read_cnt) {
			assertTRUE(SetEvent(rwlock->__write_ev));
		}
		assertTRUE(SetEvent(rwlock->__wait_ev));
	}
#else
	assertTRUE(pthread_rwlock_unlock(rwlock) == 0);
#endif
}

void rwlockClose(RWLock_t* rwlock) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(CloseHandle(rwlock->__read_ev));
	assertTRUE(CloseHandle(rwlock->__write_ev));
	assertTRUE(CloseHandle(rwlock->__wait_ev));
#else
	assertTRUE(pthread_rwlock_destroy(rwlock) == 0);
#endif
}

/* semaphore */
SemId_t semaphoreCreate(const char* name, unsigned short val) {
#if defined(_WIN32) || defined(_WIN64)
	SemId_t semid = CreateSemaphoreA(NULL, val, 0x7fffffff, name);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		assertTRUE(CloseHandle(semid));
		return NULL;
	}
	return semid;
#else
	/* max init value at last SEM_VALUE_MAX(32767) */
	return sem_open(name, O_CREAT | O_EXCL, 0666, val);
	/* mac os x has deprecated sem_init */
#endif
}

SemId_t semaphoreOpen(const char* name) {
#if defined(_WIN32) || defined(_WIN64)
	return OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
#else
	return sem_open(name, 0);
#endif
}

BOOL semaphoreTryWait(SemId_t id) {
#if defined(_WIN32) || defined(_WIN64)
	return WaitForSingleObject(id, 0) == WAIT_OBJECT_0;
#else
	return sem_trywait(id) == 0;
#endif
}

void semaphoreWait(SemId_t id) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(WaitForSingleObject(id, INFINITE) == WAIT_OBJECT_0);
#else
	assertTRUE(sem_wait(id) == 0);
#endif
}

void semaphorePost(SemId_t id) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(ReleaseSemaphore(id, 1, NULL));
#else
	assertTRUE(sem_post(id) == 0);
#endif
}

void semaphoreClose(SemId_t id) {
#if defined(_WIN32) || defined(_WIN64)
	assertTRUE(CloseHandle(id));
#else
	assertTRUE(sem_close(id) == 0);
#endif
}

BOOL semaphoreUnlink(const char* name) {
#if defined(_WIN32) || defined(_WIN64)
	return TRUE;
#else
	return sem_unlink(name) == 0;
#endif
}

/* once call */
#if defined(_WIN32) || defined(_WIN64)
BOOL CALLBACK __win32InitOnceCallback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
	void(*callback)(void) = (void(*)(void))Parameter;
	callback();
	return TRUE;
}
#endif

BOOL initonceCall(InitOnce_t* once, void(*callback)(void)) {
#if defined(_WIN32) || defined(_WIN64)
	return InitOnceExecuteOnce(once, __win32InitOnceCallback, callback, NULL);
#else
	int res = pthread_once(once, callback);
	if (res) {
		errno = res;
		return FALSE;
	}
	return TRUE;
#endif
}

#ifdef	__cplusplus
}
#endif
