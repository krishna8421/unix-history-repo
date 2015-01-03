/*
 * Copyright (c) 2014 The FreeBSD Foundation.
 * Copyright (C) 2005 David Xu <davidxu@freebsd.org>.
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>.
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <aio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "thr_private.h"

#ifdef SYSCALL_COMPAT
extern int __fcntl_compat(int, int, ...);
#endif

/*
 * Cancellation behavior:
 *   If thread is canceled, no socket is created.
 */
static int
__thr_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_accept(s, addr, addrlen);
	_thr_cancel_leave(curthread, ret == -1);

 	return (ret);
}

/*
 * Cancellation behavior:
 *   If thread is canceled, no socket is created.
 */
static int
__thr_accept4(int s, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_accept4(s, addr, addrlen, flags);
	_thr_cancel_leave(curthread, ret == -1);

 	return (ret);
}

static int
__thr_aio_suspend(const struct aiocb * const iocbs[], int niocb, const struct
    timespec *timeout)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_aio_suspend(iocbs, niocb, timeout);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   According to manual of close(), the file descriptor is always deleted.
 *   Here, thread is only canceled after the system call, so the file
 *   descriptor is always deleted despite whether the thread is canceled
 *   or not.
 */
static int
__thr_close(int fd)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter2(curthread, 0);
	ret = __sys_close(fd);
	_thr_cancel_leave(curthread, 1);
	
	return (ret);
}

/*
 * Cancellation behavior:
 *   If the thread is canceled, connection is not made.
 */
static int
__thr_connect(int fd, const struct sockaddr *name, socklen_t namelen)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_connect(fd, name, namelen);
	_thr_cancel_leave(curthread, ret == -1);

 	return (ret);
}


/*
 * Cancellation behavior:
 *   If thread is canceled, file is not created.
 */
static int
__thr_creat(const char *path, mode_t mode)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_creat(path, mode);
	_thr_cancel_leave(curthread, ret == -1);
	
	return (ret);
}

/*
 * Cancellation behavior:
 *   According to specification, only F_SETLKW is a cancellation point.
 *   Thread is only canceled at start, or canceled if the system call
 *   is failure, this means the function does not generate side effect
 *   if it is canceled.
 */
static int
__thr_fcntl(int fd, int cmd, ...)
{
	struct pthread *curthread;
	int ret;
	va_list	ap;

	curthread = _get_curthread();
	va_start(ap, cmd);
	if (cmd == F_OSETLKW || cmd == F_SETLKW) {
		_thr_cancel_enter(curthread);
#ifdef SYSCALL_COMPAT
		ret = __fcntl_compat(fd, cmd, va_arg(ap, void *));
#else
		ret = __sys_fcntl(fd, cmd, va_arg(ap, void *));
#endif
		_thr_cancel_leave(curthread, ret == -1);
	} else {
#ifdef SYSCALL_COMPAT
		ret = __fcntl_compat(fd, cmd, va_arg(ap, void *));
#else
		ret = __sys_fcntl(fd, cmd, va_arg(ap, void *));
#endif
	}
	va_end(ap);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled after system call.
 */
static int
__thr_fsync(int fd)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter2(curthread, 0);
	ret = __sys_fsync(fd);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled after system call.
 */
static int
__thr_msync(void *addr, size_t len, int flags)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter2(curthread, 0);
	ret = __sys_msync(addr, len, flags);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}

static int
__thr_nanosleep(const struct timespec *time_to_sleep,
    struct timespec *time_remaining)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_nanosleep(time_to_sleep, time_remaining);
	_thr_cancel_leave(curthread, 1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   If the thread is canceled, file is not opened.
 */
static int
__thr_open(const char *path, int flags,...)
{
	struct pthread *curthread;
	int mode, ret;
	va_list	ap;

	/* Check if the file is being created: */
	if ((flags & O_CREAT) != 0) {
		/* Get the creation mode: */
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
	}
	
	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_open(path, flags, mode);
	_thr_cancel_leave(curthread, ret == -1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   If the thread is canceled, file is not opened.
 */
static int
__thr_openat(int fd, const char *path, int flags, ...)
{
	struct pthread *curthread;
	int mode, ret;
	va_list	ap;

	
	/* Check if the file is being created: */
	if ((flags & O_CREAT) != 0) {
		/* Get the creation mode: */
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
	}
	
	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_openat(fd, path, flags, mode);
	_thr_cancel_leave(curthread, ret == -1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns something,
 *   the thread is not canceled.
 */
static int
__thr_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_poll(fds, nfds, timeout);
	_thr_cancel_leave(curthread, ret == -1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns something,
 *   the thread is not canceled.
 */
static int
__thr_pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
	const struct timespec *timo, const sigset_t *mask)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_pselect(count, rfds, wfds, efds, timo, mask);
	_thr_cancel_leave(curthread, ret == -1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call got some data, 
 *   the thread is not canceled.
 */
static ssize_t
__thr_read(int fd, void *buf, size_t nbytes)
{
	struct pthread *curthread;
	ssize_t	ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_read(fd, buf, nbytes);
	_thr_cancel_leave(curthread, ret == -1);

	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call got some data, 
 *   the thread is not canceled.
 */
static ssize_t
__thr_readv(int fd, const struct iovec *iov, int iovcnt)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_readv(fd, iov, iovcnt);
	_thr_cancel_leave(curthread, ret == -1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call got some data, 
 *   the thread is not canceled.
 */
static ssize_t
__thr_recvfrom(int s, void *b, size_t l, int f, struct sockaddr *from,
    socklen_t *fl)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_recvfrom(s, b, l, f, from, fl);
	_thr_cancel_leave(curthread, ret == -1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call got some data, 
 *   the thread is not canceled.
 */
static ssize_t
__thr_recvmsg(int s, struct msghdr *m, int f)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_recvmsg(s, m, f);
	_thr_cancel_leave(curthread, ret == -1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns something,
 *   the thread is not canceled.
 */
static int 
__thr_select(int numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_select(numfds, readfds, writefds, exceptfds, timeout);
	_thr_cancel_leave(curthread, ret == -1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call sent
 *   data, the thread is not canceled.
 */
static ssize_t
__thr_sendmsg(int s, const struct msghdr *m, int f)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_sendmsg(s, m, f);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call sent some
 *   data, the thread is not canceled.
 */
static ssize_t
__thr_sendto(int s, const void *m, size_t l, int f, const struct sockaddr *t,
    socklen_t tl)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_sendto(s, m, l, f, t, tl);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

static unsigned int
__thr_sleep(unsigned int seconds)
{
	struct pthread *curthread;
	unsigned int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_sleep(seconds);
	_thr_cancel_leave(curthread, 1);
	return (ret);
}

static int
__thr_system(const char *string)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_system(string);
	_thr_cancel_leave(curthread, 1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   If thread is canceled, the system call is not completed,
 *   this means not all bytes were drained.
 */
static int
__thr_tcdrain(int fd)
{
	struct pthread *curthread;
	int ret;
	
	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_tcdrain(fd);
	_thr_cancel_leave(curthread, ret == -1);
	return (ret);
}

static int
__thr_usleep(useconds_t useconds)
{
	struct pthread *curthread;
	int ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_usleep(useconds);
	_thr_cancel_leave(curthread, 1);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns
 *   a child pid, the thread is not canceled.
 */
static pid_t
__thr_wait(int *istat)
{
	struct pthread *curthread;
	pid_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_wait(istat);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns
 *   a child pid, the thread is not canceled.
 */
static pid_t
__thr_wait3(int *status, int options, struct rusage *rusage)
{
	struct pthread *curthread;
	pid_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_wait3(status, options, rusage);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns
 *   a child pid, the thread is not canceled.
 */
static pid_t
__thr_wait4(pid_t pid, int *status, int options, struct rusage *rusage)
{
	struct pthread *curthread;
	pid_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_wait4(pid, status, options, rusage);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the system call returns
 *   a child pid, the thread is not canceled.
 */
static pid_t
__thr_waitpid(pid_t wpid, int *status, int options)
{
	struct pthread *curthread;
	pid_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __libc_waitpid(wpid, status, options);
	_thr_cancel_leave(curthread, ret <= 0);
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the thread wrote some data,
 *   it is not canceled.
 */
static ssize_t
__thr_write(int fd, const void *buf, size_t nbytes)
{
	struct pthread *curthread;
	ssize_t	ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_write(fd, buf, nbytes);
	_thr_cancel_leave(curthread, (ret <= 0));
	return (ret);
}

/*
 * Cancellation behavior:
 *   Thread may be canceled at start, but if the thread wrote some data,
 *   it is not canceled.
 */
static ssize_t
__thr_writev(int fd, const struct iovec *iov, int iovcnt)
{
	struct pthread *curthread;
	ssize_t ret;

	curthread = _get_curthread();
	_thr_cancel_enter(curthread);
	ret = __sys_writev(fd, iov, iovcnt);
	_thr_cancel_leave(curthread, (ret <= 0));
	return (ret);
}

void
__thr_interpose_libc(void)
{

	__error_selector = __error_threaded;
#define	SLOT(name)					\
	*(__libc_interposing_slot(INTERPOS_##name)) =	\
	    (interpos_func_t)__thr_##name;
	SLOT(accept);
	SLOT(accept4);
	SLOT(aio_suspend);
	SLOT(close);
	SLOT(connect);
	SLOT(creat);
	SLOT(fcntl);
	SLOT(fsync);
	SLOT(fork);
	SLOT(msync);
	SLOT(nanosleep);
	SLOT(open);
	SLOT(openat);
	SLOT(poll);
	SLOT(pselect);
	SLOT(raise);
	SLOT(read);
	SLOT(readv);
	SLOT(recvfrom);
	SLOT(recvmsg);
	SLOT(select);
	SLOT(sendmsg);
	SLOT(sendto);
	SLOT(setcontext);
	SLOT(sigaction);
	SLOT(sigprocmask);
	SLOT(sigsuspend);
	SLOT(sigwait);
	SLOT(sigtimedwait);
	SLOT(sigwaitinfo);
	SLOT(swapcontext);
	SLOT(system);
	SLOT(sleep);
	SLOT(tcdrain);
	SLOT(usleep);
	SLOT(pause);
	SLOT(wait);
	SLOT(wait3);
	SLOT(wait4);
	SLOT(waitpid);
	SLOT(write);
	SLOT(writev);
#undef SLOT
	*(__libc_interposing_slot(
	    INTERPOS__pthread_mutex_init_calloc_cb)) =
	    (interpos_func_t)_pthread_mutex_init_calloc_cb;
}
