dnl config.m4 for extension swoole

dnl  +----------------------------------------------------------------------
dnl  | 给 configure 提供了可选项，和在运行 ./configure --help 时显示的帮助文本
dnl  +----------------------------------------------------------------------

dnl PHP_ARG_WITH() 用于取得参数的选项，例如扩展所需库或程序的位置

PHP_ARG_WITH(swoole, swoole support,
[  --with-swoole           With swoole support])

dnl PHP_ARG_ENABLE() 用于代表简单标志的选项
dnl 开启debug将会在编译的时候输出警告等
PHP_ARG_ENABLE(swoole-debug, whether to enable swoole debug,
[  --enable-swoole-debug   Enable swoole debug], [enable_swoole_debug="yes"])

PHP_ARG_ENABLE(sockets, enable sockets support,
[  --enable-sockets        Do you have sockets extension?], no, no)

PHP_ARG_ENABLE(ringbuffer, enable ringbuffer shared memory pool support,
[  --enable-ringbuffer     Use ringbuffer memory pool?], no, no)

PHP_ARG_ENABLE(async_mysql, enable async_mysql support,
[  --enable-async-mysql    Do you have mysqli and mysqlnd?], no, no)

PHP_ARG_ENABLE(openssl, enable openssl support,
[  --enable-openssl        Use openssl?], no, no)



dnl  +----------------------------------------------------------------------
dnl  | 宏定义，检测mysql扩展
dnl  +----------------------------------------------------------------------

AC_DEFUN([SWOOLE_HAVE_PHP_EXT], [
    dnl 传入第一个参数,如mysqli
    extname=$1 
    dnl 字符串转大写，并字符连接，如PHP_MYSQLI
    haveext=[PHP_]translit($1,a-z_-,A-Z__) 

    AC_MSG_CHECKING([【swoole】 for ext/$extname support]) 

    dnl 命令返回值或文件是否存在，$PHP_EXECUTABLE环境变量如/usr/bin/php
    if test -x "$PHP_EXECUTABLE"; then
        dnl 如果/usr/bin/php存在
        dnl 拼接命令，如 /usr/bin/php -m | /usr/bin/grep -E mysqli
        grepext=`$PHP_EXECUTABLE -m | $EGREP ^$extname\$`
        if test "$grepext" = "$extname"; then
            AC_MSG_RESULT([yes])
            $2
        else
            AC_MSG_RESULT([no])
            $3
        fi
    elif test "$haveext" != "no" && test "x$haveext" != "x"; then
        dnl 如果第一参数不是no并且存在
        AC_MSG_RESULT([yes])
        $2
    else
        AC_MSG_RESULT([no])
        $3
    fi
])


dnl  +----------------------------------------------------------------------
dnl  | 检测是否支持sched.h对cpu的操作
dnl  +----------------------------------------------------------------------

AC_DEFUN([AC_SWOOLE_CPU_AFFINITY],
[
    AC_MSG_CHECKING([【swoole】 for cpu affinity])
    dnl 检验语法 AC_TRY_COMPILE (includes, function-body, [action-if-found [, action-if-not-found]])
    AC_TRY_COMPILE(
    [
		#include <sched.h>
    ], [
		cpu_set_t cpu_set;
		CPU_ZERO(&cpu_set); // CPU_ZERO用于清空cpu_set_t类型对象的内容
    ], [
        AC_DEFINE([HAVE_CPU_AFFINITY], 1, [【swoole】 cpu affinity?])
        AC_MSG_RESULT([yes])
    ], [
        AC_MSG_RESULT([no])
    ])
])


dnl  +----------------------------------------------------------------------
dnl  | 检测是否支持套接字操作
dnl  +----------------------------------------------------------------------

AC_DEFUN([AC_SWOOLE_HAVE_REUSEPORT],
[
    AC_MSG_CHECKING([【swoole】 for socket REUSEPORT])
    AC_TRY_COMPILE(
    [
		#include <sys/socket.h>
    ], [
        int val = 1;
		setsockopt(0, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)); // 获取或者设置与某个套接字关联的选项
    ], [
        AC_DEFINE([HAVE_REUSEPORT], 1, [have SO_REUSEPORT?])
        AC_MSG_RESULT([yes])
    ], [
        AC_MSG_RESULT([no])
    ])
])


dnl 检测语法编译器 AC_COMPILE_IFELSE (input, [action-if-true], [action-if-false])
AC_MSG_CHECKING([【swoole】 if compiling with clang])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([], [[
        #ifndef __clang__
            not clang
        #endif
    ]])],
    [CLANG=yes], [CLANG=no]
)
AC_MSG_RESULT([$CLANG])
if test "$CLANG" = "yes"; then
    dnl 预定义输出变量： CFLAGS为C编译器提供的调试和优化选项
    CFLAGS="$CFLAGS -std=gnu89"
fi
CFLAGS="-Wall -pthread $CFLAGS"
LDFLAGS="$LDFLAGS -lpthread"

dnl PHP_ADD_LIBRARY() 用于将pthread多线程类库链接进入扩展
PHP_ADD_LIBRARY(pthread)
dnl PHP_SUBST() 用于说明这个扩展编译成动态链接库的形式
PHP_SUBST(SWOOLE_SHARED_LIBADD)

dnl AC_ARG_ENABLE (feature, help-string [, action-if-given [, action-if-not-given]])
AC_ARG_ENABLE(debug, 
    [--enable-debug,  compile with debug symbols],
    [PHP_DEBUG=$enableval],
    [PHP_DEBUG=0]
)
  
dnl 检测 PHP_ARG_ENABLE 设置的 swoole-debug 标志值
if test "$PHP_SWOOLE_DEBUG" != "no"; then
    AC_DEFINE(SW_DEBUG, 1, [【swoole】 do we enable swoole debug])
fi

if test "$PHP_SOCKETS" = "yes"; then
    AC_DEFINE(SW_SOCKETS, 1, [【swoole】 enable sockets support])
fi

if test "$PHP_RINGBUFFER" = "yes"; then
    AC_DEFINE(SW_USE_RINGBUFFER, 1, [【swoole】 enable ringbuffer support])
fi

if test "$PHP_ASYNC_MYSQL" = "yes"; then
    AC_DEFINE(SW_ASYNC_MYSQL, 1, [【swoole】 enable async_mysql support])
fi

dnl 检测是否支持sched.h对cpu的操作
AC_SWOOLE_CPU_AFFINITY

dnl 检测是否支持套接字操作
AC_SWOOLE_HAVE_REUSEPORT

dnl 检测mysql扩展
SWOOLE_HAVE_PHP_EXT([mysqli], [
    AC_DEFINE(SW_HAVE_MYSQLI, 1, [【swoole】 have mysqli])
])

dnl 检测mysqlnd扩展
SWOOLE_HAVE_PHP_EXT([mysqlnd], [
    AC_DEFINE(SW_HAVE_MYSQLND, 1, [【swoole】 have mysqlnd])
])

dnl AC_CHECK_LIB (library, function [, action-if-found [, action-if-not-found [, other-libraries]]])
dnl 检测对应库函数
dnl accept4处理套接口连接
AC_CHECK_LIB(c, accept4, AC_DEFINE(HAVE_ACCEPT4, 1, [【swoole】 have accept4]))
dnl signalfd信号处理抽象为文件描述符
AC_CHECK_LIB(c, signalfd, AC_DEFINE(HAVE_SIGNALFD, 1, [【swoole】 have signalfd]))
dnl timerfd_create用于创建一个定时器文件
AC_CHECK_LIB(c, timerfd_create, AC_DEFINE(HAVE_TIMERFD, 1, [【swoole】 have timerfd]))
dnl eventfd具体与pipe有点像，用来完成两个线程之间事件触发
AC_CHECK_LIB(c, eventfd, AC_DEFINE(HAVE_EVENTFD, 1, [【swoole】 have eventfd]))
dnl epoll_create生成一个epoll专用的文件描述符
AC_CHECK_LIB(c, epoll_create, AC_DEFINE(HAVE_EPOLL, 1, [have epoll]))
dnl sendfile函数进行零拷贝发送文件，实现高效数据传输
AC_CHECK_LIB(c, sendfile,AC_DEFINE(HAVE_SENDFILE, 1, [【swoole】 have sendfile]))
dnl kqueue 支持多种类型的文件描述符
AC_CHECK_LIB(c, kqueue, AC_DEFINE(HAVE_KQUEUE, 1, [【swoole】 have kqueue]))
dnl daemon()函数主要用于希望脱离控制台，以守护进程形式在后台运行的程序
AC_CHECK_LIB(c, daemon, AC_DEFINE(HAVE_DAEMON, 1, [【swoole】 have daemon]))
dnl 建立唯一临时文件
AC_CHECK_LIB(c, mkostemp, AC_DEFINE(HAVE_MKOSTEMP, 1, [【swoole】 have mkostemp]))
dnl inotify_init() 是用于创建一个 inotify 实例的系统调用，并返回一个指向该实例的文件描述符。
AC_CHECK_LIB(c, inotify_init, AC_DEFINE(HAVE_INOTIFY, 1, [【swoole】 have inotify]))
dnl 与 inotify_init 相似，并带有附加标志。如果这些附加标志没有指定，将采用与 inotify_init 相同的值。
AC_CHECK_LIB(c, inotify_init1, AC_DEFINE(HAVE_INOTIFY_INIT1, 1, [【swoole】 have inotify_init1]))
dnl pthread_rwlock_init是linux系统用于线程管理的一个函数。
AC_CHECK_LIB(pthread, pthread_rwlock_init, AC_DEFINE(HAVE_RWLOCK, 1, [【swoole】 have pthread_rwlock_init]))
dnl 线程锁
AC_CHECK_LIB(pthread, pthread_spin_lock, AC_DEFINE(HAVE_SPINLOCK, 1, [【swoole】 have pthread_spin_lock]))
dnl 当线程试图获取一个已加锁的互斥变量时，pthread_mutex_timedlock互斥量原语允许绑定线程阻塞的时间。
AC_CHECK_LIB(pthread, pthread_mutex_timedlock, AC_DEFINE(HAVE_MUTEX_TIMEDLOCK, 1, [【swoole】 have pthread_mutex_timedlock]))
dnl 用于多线程的同步
AC_CHECK_LIB(pthread, pthread_barrier_init, AC_DEFINE(HAVE_PTHREAD_BARRIER, 1, [【swoole】 have pthread_barrier_init]))
dnl 初始化SSL算法库函数
AC_CHECK_LIB(ssl, SSL_library_init, AC_DEFINE(HAVE_OPENSSL, 1, [【swoole】 have openssl]))
dnl 将一个正则表达式编译成一个内部表示，在匹配多个字符串时，可以加速匹配
AC_CHECK_LIB(pcre, pcre_compile, AC_DEFINE(HAVE_PCRE, 1, [【swoole】 have pcre]))
dnl zlib库gzgets从压缩文件中读取字节，直到len-1个字符被读取，或者一个换行字符被读取并传给buf，或者一个end-of-file条件被触发。
AC_CHECK_LIB(z, gzgets, [
    AC_DEFINE(SW_HAVE_ZLIB, 1, [【swoole】 have zlib])
    PHP_ADD_LIBRARY(z, 1, SWOOLE_SHARED_LIBADD)
])

if test `uname` = "Darwin" ; then
    dnl 如果是mac系统
    dnl clock_gettime用于计算精度和纳秒
    AC_CHECK_LIB(c, clock_gettime, AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [【swoole】have clock_gettime]))
    dnl aio_read异步读取
    AC_CHECK_LIB(c, aio_read, AC_DEFINE(HAVE_GCC_AIO, 1, [【swoole】have gcc aio]))
else
    AC_CHECK_LIB(rt, clock_gettime, AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [【swoole】have clock_gettime]))
    AC_CHECK_LIB(rt, aio_read, AC_DEFINE(HAVE_GCC_AIO, 1, [【swoole】have gcc aio]))
    PHP_ADD_LIBRARY(rt, 1, SWOOLE_SHARED_LIBADD)
fi

PHP_ADD_LIBRARY(pthread, 1, SWOOLE_SHARED_LIBADD)

dnl 检测php_openssl
if test "$PHP_OPENSSL" = "yes"; then
    AC_DEFINE(SW_USE_OPENSSL, 1, [【swoole】enable openssl support])
    PHP_ADD_LIBRARY(ssl, 1, SWOOLE_SHARED_LIBADD)
    PHP_ADD_LIBRARY(crypt, 1, SWOOLE_SHARED_LIBADD)
    PHP_ADD_LIBRARY(crypto, 1, SWOOLE_SHARED_LIBADD)
fi

swoole_source_file="
    swoole.c \
    swoole_server.c \
    swoole_atomic.c \
    swoole_lock.c \
    swoole_client.c \
    swoole_http_client.c \
    swoole_event.c \
    swoole_timer.c \
    swoole_async.c \
    swoole_process.c \
    swoole_buffer.c \
    swoole_table.c \
    swoole_http.c \
    swoole_websocket.c \
    swoole_mysql.c \
    src/core/base.c \
    src/core/log.c \
    src/core/hashmap.c \
    src/core/RingQueue.c \
    src/core/Channel.c \
    src/core/string.c \
    src/core/array.c \
    src/core/socket.c \
    src/core/list.c \
    src/memory/ShareMemory.c \
    src/memory/MemoryGlobal.c \
    src/memory/RingBuffer.c \
    src/memory/FixedPool.c \
    src/memory/Malloc.c \
    src/memory/Table.c \
    src/memory/Buffer.c \
    src/factory/Factory.c \
    src/factory/FactoryThread.c \
    src/factory/FactoryProcess.c \
    src/reactor/ReactorBase.c \
    src/reactor/ReactorSelect.c \
    src/reactor/ReactorPoll.c \
    src/reactor/ReactorEpoll.c \
    src/reactor/ReactorKqueue.c \
    src/pipe/PipeBase.c \
    src/pipe/PipeEventfd.c \
    src/pipe/PipeUnsock.c \
    src/lock/Semaphore.c \
    src/lock/Mutex.c \
    src/lock/RWLock.c \
    src/lock/SpinLock.c \
    src/lock/FileLock.c \
    src/network/Server.c \
    src/network/TaskWorker.c \
    src/network/Client.c \
    src/network/Connection.c \
    src/network/ProcessPool.c \
    src/network/ThreadPool.c \
    src/network/ReactorThread.c \
    src/network/ReactorProcess.c \
    src/network/Manager.c \
    src/network/Worker.c \
    src/network/EventTimer.c \
    src/os/base.c \
    src/os/linux_aio.c \
    src/os/gcc_aio.c \
    src/os/msg_queue.c \
    src/os/sendfile.c \
    src/os/signal.c \
    src/os/timer.c \
    src/protocol/Base.c \
    src/protocol/SSL.c \
    src/protocol/Http.c \
    src/protocol/WebSocket.c \
    src/protocol/Mqtt.c \
    src/protocol/Base64.c \
    thirdparty/php_http_parser.c \
    thirdparty/multipart_parser.c \
    "        


dnl 最后，将扩展及其所需文件等信息传给构建系统
dnl 第一个参数是扩展的名称，和包含它的目录同名。第二个参数是做为扩展的一部分的所有源文件的列表。第三个参数总是 $ext_shared
PHP_NEW_EXTENSION(swoole, $swoole_source_file, $ext_shared)

dnl 添加所需路径
PHP_ADD_INCLUDE([$ext_srcdir/include])

PHP_ADD_BUILD_DIR($ext_builddir/src/core)
PHP_ADD_BUILD_DIR($ext_builddir/src/memory)
PHP_ADD_BUILD_DIR($ext_builddir/src/factory)
PHP_ADD_BUILD_DIR($ext_builddir/src/reactor)
PHP_ADD_BUILD_DIR($ext_builddir/src/pipe)
PHP_ADD_BUILD_DIR($ext_builddir/src/lock)
PHP_ADD_BUILD_DIR($ext_builddir/src/os)
PHP_ADD_BUILD_DIR($ext_builddir/src/network)
PHP_ADD_BUILD_DIR($ext_builddir/src/protocol)
PHP_ADD_BUILD_DIR($ext_builddir/thirdparty)

