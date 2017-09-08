#ifndef SW_ATOMIC_H_
#define SW_ATOMIC_H_

#if defined(__x86_64__)
#define SW_ATOMIC_64_LEN                     (sizeof("-9223372036854775808") - 1)
typedef volatile int64_t atomic_int64_t;
typedef volatile uint64_t atomic_uint64_t;
#endif

#define SW_ATOMIC_32_LEN                      (sizeof("-2147483648") - 1)
typedef volatile int32_t atomic_int32_t;
typedef volatile uint32_t atomic_uint32_t;
typedef atomic_uint32_t  sw_atomic_t;

// swoole使用了gcc提供的__sync_*系列的build-in函数来提供加减和逻辑运算的原子操作
// 如果lock的值等于old的值，将set的值写入lock。如果相等并写入成功则返回true
#define sw_atomic_cmp_set(lock, old, set) __sync_bool_compare_and_swap(lock, old, set)
// 将value的值增加add，返回增加前的值
#define sw_atomic_fetch_add(value, add)   __sync_fetch_and_add(value, add)
// 将value的值减去sub，返回减去前的值
#define sw_atomic_fetch_sub(value, sub)   __sync_fetch_and_sub(value, sub)
// 发出一个full barrier。保证指令执行的顺序合理
#define sw_atomic_memory_barrier()        __sync_synchronize()

// 通过一番查询，这个调用能让cpu等待一段时间，这个时间由处理器定义。
#ifdef __arm__
#define sw_atomic_cpu_pause()             __asm__ __volatile__ ("NOP");
#elif defined(__x86_64__)
#define sw_atomic_cpu_pause()             __asm__ __volatile__ ("pause")
#else
#define sw_atomic_cpu_pause()
#endif

#define sw_spinlock_release(lock)         __sync_lock_release(lock)

#endif
