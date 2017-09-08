#include "swoole.h"

// 分别用于初始化、分配空间、释放空间、销毁内存池
static void swFixedPool_init(swFixedPool *object);
static void* swFixedPool_alloc(swMemoryPool *pool, uint32_t size);
static void swFixedPool_free(swMemoryPool *pool, void *ptr);
static void swFixedPool_destroy(swMemoryPool *pool);

void swFixedPool_debug_slice(swFixedPool_slice *slice);

/**
 * create new FixedPool, random alloc/free fixed size memory
 */
// 第一个参数size为每一个小块的大小
// 第二个参数trunk_size为小块的总数
// 第三个参数shared代表该内存池是否为共享内存
// 返回的是被创建的内存池指针
swMemoryPool* swFixedPool_new(uint32_t slice_num, uint32_t slice_size, uint8_t shared)
{
    size_t size = slice_size * slice_num + slice_num * sizeof(swFixedPool_slice); // slice大小*slice数量+slice数量*
    size_t alloc_size = size + sizeof(swFixedPool) + sizeof(swMemoryPool); // FixedPool头部大小+MemoryPool头部大小
    void *memory = (shared == 1) ? sw_shm_malloc(alloc_size) : sw_malloc(alloc_size); // 根据是否是共享内存来判定使用哪种分配内存的方法

    swFixedPool *object = memory;
    memory += sizeof(swFixedPool);
    bzero(object, sizeof(swFixedPool));

    object->shared = shared;
    object->slice_num = slice_num;
    object->slice_size = slice_size;
    object->size = size;

    swMemoryPool *pool = memory;
    memory += sizeof(swMemoryPool);
    pool->object = object;
    pool->alloc = swFixedPool_alloc;
    pool->free = swFixedPool_free;
    pool->destroy = swFixedPool_destroy;

    object->memory = memory;

    /**
     * init linked list
     */
    swFixedPool_init(object);

    return pool;
}

/**
 * create new FixedPool, Using the given memory
 */
swMemoryPool* swFixedPool_new2(uint32_t slice_size, void *memory, size_t size)
{
    swFixedPool *object = memory;
    memory += sizeof(swFixedPool);
    bzero(object, sizeof(swFixedPool));

    object->slice_size = slice_size;
    object->size = size - sizeof(swMemoryPool) - sizeof(swFixedPool);
    object->slice_num = object->size / (slice_size + sizeof(swFixedPool_slice));

    swMemoryPool *pool = memory;
    memory += sizeof(swMemoryPool);
    bzero(pool, sizeof(swMemoryPool));

    pool->object = object;
    pool->alloc = swFixedPool_alloc;
    pool->free = swFixedPool_free;
    pool->destroy = swFixedPool_destroy;

    object->memory = memory;

    /**
     * init linked list
     */
    swFixedPool_init(object);

    return pool;
}

/**
 * linked list
 */
static void swFixedPool_init(swFixedPool *object)
{
    swFixedPool_slice *slice;
    void *cur = object->memory;
    void *max = object->memory + object->size;
    do
    {
        // 初始化一个slice大小的空间
        slice = (swFixedPool_slice *) cur;
        bzero(slice, sizeof(swFixedPool_slice));
        if (object->head != NULL)
        {
            // 插入到链表的头部
            object->head->pre = slice;
            slice->next = object->head;
        }
        else
            object->tail = slice;

        object->head = slice;
        cur += (sizeof(swFixedPool_slice) + object->slice_size);

        if (cur < max)
            slice->pre = (swFixedPool_slice *) cur;
        else
        {
            slice->pre = NULL;
            break;
        }

    } while (1);
}

// 因为FixedPool的内存块大小是固定的，所以函数中第二个参数size只是为了符合swMemoryPool中的声明
static void* swFixedPool_alloc(swMemoryPool *pool, uint32_t size)
{   
    swFixedPool *object = pool->object;
    swFixedPool_slice *slice;
    // 获取内存池链表首部的节点
    slice = object->head;
    // 如果未被占用，则将该节点的下一个节点移到首部，并将该节点移动到尾部，标记该节点为占用状态，返回该节点的数据域
    if (slice->lock == 0)
    {
        slice->lock = 1;
        object->slice_use ++;
        /**
         * move next slice to head (idle list)
         */
        object->head = slice->next;
        slice->next->pre = NULL;

        /*
         * move this slice to tail (busy list)
         */
        object->tail->next = slice;
        slice->next = NULL;
        slice->pre = object->tail;
        object->tail = slice;

        return slice->data;
    }
    else
        return NULL; // 判断该节点是否被占用，如果被占用，说明内存池已满（因为所有被占用的节点都会被放到尾部）
}

// 第二个参数为需要释放的数据域
static void swFixedPool_free(swMemoryPool *pool, void *ptr)
{
    swFixedPool *object = pool->object;
    swFixedPool_slice *slice;

    assert(ptr > object->memory && ptr < object->memory + object->size);
    // 通过移动ptr指针获得slice对象
    slice = ptr - sizeof(swFixedPool_slice);
    if (slice->lock)
        object->slice_use--;
    slice->lock = 0; // 将占用标记lock置为0
    //list head, AB 如果该节点为头节点，则直接返回
    if (slice->pre == NULL)
        return;
    //list tail, DE 如果不是头节点，则将该节点移动到链表头部
    if (slice->next == NULL)
    {
        slice->pre->next = NULL;
        object->tail = slice->pre;
    }
    //middle BCD
    else
    {
        slice->pre->next = slice->next;
        slice->next->pre = slice->pre;
    }

    slice->pre = NULL;
    slice->next = object->head;
    object->head->pre = slice;
    object->head = slice;
}

static void swFixedPool_destroy(swMemoryPool *pool)
{
    swFixedPool *object = pool->object;
    if (object->shared)
    {
        sw_shm_free(object);
    }
    else
    {
        sw_free(object);
    }
}

void swFixedPool_debug(swMemoryPool *pool)
{
    int line = 0;
    swFixedPool *object = pool->object;
    swFixedPool_slice *slice = object->head;

    printf("===============================%s=================================\n", __FUNCTION__);
    while (slice != NULL)
    {
        if (slice->next == slice)
        {
            printf("-------------------@@@@@@@@@@@@@@@@@@@@@@----------------\n");

        }
        printf("#%d\t", line);
        swFixedPool_debug_slice(slice);

        slice = slice->next;
        line++;
        if (line > 100)
            break;
    }
}

void swFixedPool_debug_slice(swFixedPool_slice *slice)
{
    printf("Slab[%p]\t", slice);
    printf("pre=%p\t", slice->pre);
    printf("next=%p\t", slice->next);
    printf("tag=%d\t", slice->lock);
    printf("data=%p\n", slice->data);
}
