#ifndef CRUX_COMMON_H
#define CRUX_COMMON_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

typedef ssize_t (*xio_fn) (int fd, void *buf, size_t len, int timeoutms);

/**
 * @brief  Exits the current running task or process
 *
 * @param  ec  exit code
 * @return  0 on success, -errno on error
 */
extern void
xexit (int ec);

/**
 * @brief  Gets the clock for the current task
 *
 * This clock is updated just prior to the task regaining context. This is
 * useful for planning timed events the need to run at a somewhat constant
 * interval regardless of time taken by the task itself.
 *
 * Note, this a monotonic clock.
 *
 * @return  clock object reference
 */
extern const struct xclock *
xclock (void);

/**
 * @brief  Sleeps the current context
 *
 * If the current context is a task, this will yield context to back to the
 * hub. Otherwise, this acts a normal thread sleep.
 *
 * @param  ms  number of milliseconds to sleep for
 * @return  0 on success, -errno on error
 */
extern int
xsleep (unsigned ms);

/**
 * @brief  Yields the current context until a signal is delivered
 *
 * @param  signum     signal number to wait for
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  signum on success, -errno on error
 */
extern int
xsignal (int signum, int timeoutms);

/**
 * @brief  Reads upto `len` bytes from the file descriptor
 *
 * This calls `read(2)`. If this results in an `EAGAIN`, the current task will
 * yield context until either the file descriptor becomes readable or the
 * timeout is reached.
 *
 * @param  fd         file descriptor to read from
 * @param  buf        buffer to read bytes into
 * @param  len        maximum number of bytes to read
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes read, -errno on error
 */
extern ssize_t
xread (int fd, void *buf, size_t len, int timeoutms);

/**
 * @brief  Reads iovec buffers from the file descriptor
 *
 * This calls `readv(2)`. If this results in an `EAGAIN`, the current task will
 * yield context until either the file descriptor becomes readable or the
 * timeout is reached.
 *
 * @param  fd         file descriptor to read from
 * @param  iov        array of iovec structures
 * @param  iovcnt     maximum number of iovec structures to read into
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes read, -errno on error
 */
extern ssize_t
xreadv (int fd, struct iovec *iov, int iovcnt, int timeoutms);

/**
 * @brief  Reads exactly `len` bytes from the file descriptor
 *
 * This calls `read(2)` until all bytes are read. If this results in an
 * `EAGAIN`, the current task will yield context until either the file
 * descriptor becomes readable or the timeout is reached.
 *
 * @param  fd         file descriptor to read from
 * @param  buf        buffer to read bytes into
 * @param  len        number of bytes to read
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes read, -errno on error
 */
extern ssize_t
xreadn (int fd, void *buf, size_t len, int timeoutms);

/**
 * @brief  Writes upto `len` bytes to the file descriptor
 *
 * This calls `write(2)`. If this results in an `EAGAIN`, the current task will
 * yield context until either the file descriptor becomes writable or the
 * timeout is reached.
 *
 * @param  fd         file descriptor to write to
 * @param  buf        buffer to write bytes from
 * @param  len        maximum number of bytes to write
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes written, -errno on error
 */
extern ssize_t
xwrite (int fd, const void *buf, size_t len, int timeoutms);

/**
 * @brief  Writes iovec structures to the file descriptor
 *
 * This calls `writev(2)`. If this results in an `EAGAIN`, the current task will
 * yield context until either the file descriptor becomes writable or the
 * timeout is reached.
 *
 * @param  fd         file descriptor to write to
 * @param  iov        array of iovec structures
 * @param  iovcnt     maximum number of iovec structures to write from
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes written, -errno on error
 */
extern ssize_t
xwritev (int fd, const struct iovec *iov, int iovcnt, int timeoutms);

/**
 * @brief  Writes exactly `len` bytes to the file descriptor
 *
 * This calls `write(2)` until all bytes are written. If this results in an
 * `EAGAIN`, the current task will yield context until either the file
 * descriptor becomes writable.
 *
 * @param  fd         file descriptor to write to
 * @param  buf        buffer to write bytes from
 * @param  len        number of bytes to write
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes written, -errno on error
 */
extern ssize_t
xwriten (int fd, const void *buf, size_t len, int timeoutms);

/**
 * @brief  Recieves a message from a socket
 *
 * This calls `recvfrom(2)`. If this results in an `EAGAIN`, the current task
 * will yield context until either the file descriptor becomes readable or the
 * timeout is reached.
 *
 * @param  s          socket file descriptor to read from
 * @param  buf        buffer to read bytes into
 * @param  len        maximum number of bytes to read
 * @param  flags      flags to pass to `recvfrom(2)`
 * @param[out]  src_addr   captures the source address if not `NULL`
 * @param[in,out]  src_len  input the maximum size of `src_addr`, output size of `src_addr`
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes read, -errno on error
 */
extern ssize_t
xrecvfrom (int s, void *buf, size_t len, int flags,
	 struct sockaddr *src_addr, socklen_t *src_len, int timeoutms);

/**
 * @brief  Sends a message to a socket
 *
 * This calls `sendto(2)`. If this results in an `EAGAIN`, the current task
 * will yield context until either the file descriptor becomes readable or the
 * timeout is reached.
 *
 * @param  s          socket file descriptor to send to
 * @param  buf        buffer to write bytes from
 * @param  len        maximum number of bytes to write
 * @param  flags      flags to pass to `sendto(2)`
 * @param[out]  src_addr   captures the source address if not `NULL`
 * @param[in,out]  src_len  input the maximum size of `src_addr`, output size of `src_addr`
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @return  number of bytes written, -errno on error
 */
extern ssize_t
xsendto (int s, const void *buf, size_t len, int flags,
	 const struct sockaddr *dest_addr, socklen_t dest_len, int timeoutms);

/**
 * @brief  Calls `fn` for all bytes in `buf`
 *
 * The callback function is expected to return either the number of bytes
 * processed or an error. This callback will be invoked until all bytes have 
 * been handled or an error is returned.
 *
 * If `timeoutms` is 0 or greater, the `xio` function will update the timeout
 * before each call to `fn` to adjust the remaining timeout period.
 *
 * @param  fd         file descriptor to pass to `fn`
 * @param  buf        buffer of bytes
 * @param  len        number of bytes in `buf`
 * @param  timeoutms  millisecond timeout or <0 for infinite
 * @param  fn         function to handle process `buf`
 * @return  the value of `len` on success, -errno on error
 */
extern ssize_t
xio (int fd, void *buf, size_t len, int timeoutms, xio_fn fn);

/**
 * @brief  Creates a non-blocking pipe pair
 *
 * This will also set the FD_CLOEXEC flag. If either of the pipe file
 * descriptors need to be shared with a child process, they should either be
 * eplicitly duplicated or have the flag removed.
 *
 * @param  fds  2-element array to capture to pipe file descriptors
 * @return  0 on success, -errno on error
 */
extern int
xpipe (int fds[static 2]);

/**
 * @brief  Opens a non-blocking socket
 *
 * This will also set the FD_CLOEXEC flag. If the socket needs to be shared
 * with a child process, it should either be eplicitly duplicated or have the
 * flag removed.
 *
 * See `socket(2)` for information about the `domain`, `type`,
 * and `protocol` parameters.
 *
 * @param  domain    communication domain (AF_INET, AF_LOCAL, etc.)
 * @param  type      socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
 * @param  protocol  protocol for the socket, or 0 for default
 * @return  >0 file descriptor on success, -errno on error
 */
extern int
xsocket (int domain, int type, int protocol);

/**
 * @brief  Accepts a connection off of the stream socket
 *
 * If the socket has no pending connections, the task will yield until one
 * becomes available or the timeout is reached.
 *
 * See `accept(2)` for information about the `s`, `addr`, and
 * `addrlen` parameters. The `timeoutms` parameter optionally
 * specifies a timeout in milliseconds for the accept. If this
 * value is less than 0, the accept will wait indefinitely.
 *
 * @param  s                socket file descriptor
 * @param[out]  addr        address of the connecting entity
 * @param[in,out]  addrlen  input the maximum size of `addr`, output size of `addr`
 * @param  timeoutms        millisecond timeout or <0 for infinite
 * @return  0 on success, -errno on error
 */
extern int
xaccept (int s, struct sockaddr *addr, socklen_t *addrlen, int timeoutms);

/**
 * @brief  Sets the O_NONBLOCK flag on the file descriptor
 *
 * When successful, reads and writes to this file descriptor will return
 * EAGAIN when doing so would block. Given that most of the concurrency
 * primatives rely on non-blocking IO, the socket creation functions will
 * have already set this flag.
 *
 * This function will pass through `fd` as-is if either `fd` is less than
 * zero (i.e. already an error), or the flag is applied successfully. 
 *
 * @param  file descriptor or error
 * @return  >0 file descriptor on success, -errno on error
 */
extern int
xunblock (int fd);

/**
 * @brief  Sets the FD_CLOEXEC flag on the file descriptor
 *
 * When successful, this file descriptor will not be duplicated in child
 * processes. It is generally preferrable to set all file descriptors to not
 * be inherited, and then explicitly duplicate file descriptors into the child
 * process, and the socket creation functions will have already set this flag.
 *
 * This function will pass through `fd` as-is if either `fd` is less than
 * zero (i.e. already an error), or the flag is applied successfully. 
 *
 * @param  file descriptor or error
 * @return  >0 file descriptor on success, -errno on error
 */
extern int
xcloexec (int fd);

#endif

