/*
 * This file is part of cyanrip.
 *
 * cyanrip is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * cyanrip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with cyanrip; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * POSIX threads, or with MSVC, which has none, the part of their API that
 * cyanrip uses, on Windows' own threads, slim reader/writer locks and
 * condition variables.  Only what cyanrip needs: default attributes, no
 * recursive or timed locking.  Like a POSIX mutex, a lock must be unlocked
 * by the thread that locked it.
 */

#pragma once

#ifndef _MSC_VER
#include <pthread.h>
#else
#include <errno.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

static inline int pthread_mutex_init(pthread_mutex_t *m, const void *attr)
{
    (void)attr;
    InitializeSRWLock(m);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
    (void)m; /* Nothing to free */
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
    AcquireSRWLockExclusive(m);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
    ReleaseSRWLockExclusive(m);
    return 0;
}

static inline int pthread_cond_init(pthread_cond_t *c, const void *attr)
{
    (void)attr;
    InitializeConditionVariable(c);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *c)
{
    (void)c; /* Nothing to free */
    return 0;
}

/* Like pthread_cond_wait(), this may return without a signal */
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    return SleepConditionVariableSRW(c, m, INFINITE, 0) ? 0 : EINVAL;
}

static inline int pthread_cond_signal(pthread_cond_t *c)
{
    WakeConditionVariable(c);
    return 0;
}

/* Windows threads return an unsigned, so the function and what it returns
 * are kept here, until pthread_join() */
typedef struct CRThread {
    HANDLE handle;
    void *(*func)(void *arg);
    void *arg;
    void *ret;
} *pthread_t;

static inline unsigned __stdcall cr_thread_start(void *opaque)
{
    pthread_t t = opaque;
    t->ret = t->func(t->arg);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const void *attr,
                                 void *(*func)(void *arg), void *arg)
{
    (void)attr;
    pthread_t t = calloc(1, sizeof(*t));
    if (!t)
        return ENOMEM;
    t->func = func;
    t->arg = arg;
    t->handle = (HANDLE)_beginthreadex(NULL, 0, cr_thread_start, t, 0, NULL);
    if (!t->handle) {
        int err = errno;
        free(t);
        return err ? err : EAGAIN;
    }
    *thread = t;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **ret)
{
    if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0)
        return EINVAL;
    if (ret)
        *ret = thread->ret;
    CloseHandle(thread->handle);
    free(thread);
    return 0;
}
#endif
