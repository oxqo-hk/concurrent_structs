#include "common.h"
#include "mpmc_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <memory.h>

//NOLINTBEGIN
#define spsc_enqueue_function(type, name) \
int name( spsc_queue_t *q, type *data, u32 len ) \
{ \
    int res = spsc_queue_prod_avail( q ); \
    res = ( res > len ) ? len : res; \
 \
    for ( int i = 0; i < res; i++ ) \
    { \
        ((type *)(q->buffer))[( q->producer + i ) % q->buffer_size] = data[i]; \
    } \
 \
    __atomic_add_fetch( &q->producer, res, __ATOMIC_SEQ_CST ); \
 \
    return res; \
}
//NOLINTEND

//NOLINTBEGIN
#define spsc_dequeue_function(type, name) \
int name( spsc_queue_t *q, type *data, u32 len ) \
{ \
    int res = spsc_queue_cons_avail( q ); \
    res = ( res > len ) ? len : res; \
 \
    for ( int i = 0; i < res; i++ ) \
    { \
        data[i] = ((type *)(q->buffer))[( q->consumer + i ) % q->buffer_size]; \
    } \
    __atomic_add_fetch( &q->consumer, res, __ATOMIC_SEQ_CST ); \
 \
    return res; \
}
//NOLINTEND

const size_t g_buf_type_size[] = {
    [Q_BUF8] = sizeof(u8),
    [Q_BUF16] = sizeof(u16),
    [Q_BUF32] = sizeof(u32),
    [Q_BUF64] = sizeof(u64),
    [Q_BUFP] = sizeof(void*),
};

spsc_queue_t *spsc_queue_new( u32 buf_size, q_buf_type_t buf_type )
{
    spsc_queue_t *res = NULL;
    defer_stack(2, on_failure);
    res = calloc( 1, sizeof( spsc_queue_t ) );
    if ( defer_add(res, &free, on_failure) )
    {
        perror( "calloc" );
        goto failure;
    }
    res->buffer = calloc( buf_size, g_buf_type_size[buf_type] );
    if ( defer_add(res->buffer, &free, on_failure))
    {
        perror( "calloc" );
        goto failure;
    }
    res->buf_type = buf_type;
    res->buffer_size = buf_size;
    return res;

failure:
    defer_release(on_failure);
    return NULL;
}

void spsc_queue_clean( spsc_queue_t *q )
{
    free( q->buffer );
    free( q );
    return;
}

CST_ALWAYS_INLINE u32 spsc_queue_cons_avail( spsc_queue_t *q )
{
    return ( __atomic_load_n( &q->producer, __ATOMIC_SEQ_CST ) - q->consumer );
}

CST_ALWAYS_INLINE u32 spsc_queue_prod_avail( spsc_queue_t *q )
{
    return ( ( ( u32 )( __atomic_load_n( &q->consumer, __ATOMIC_SEQ_CST ) + q->buffer_size ) ) - q->producer );
}

CST_ALWAYS_INLINE spsc_enqueue_function(u8, spsc_enqueue_u8)
CST_ALWAYS_INLINE spsc_enqueue_function(u16, spsc_enqueue_u16)
CST_ALWAYS_INLINE spsc_enqueue_function(u32, spsc_enqueue_u32)
CST_ALWAYS_INLINE spsc_enqueue_function(u64, spsc_enqueue_u64)
CST_ALWAYS_INLINE spsc_enqueue_function(void*, spsc_enqueue_ptr)

int spsc_enqueue( spsc_queue_t *q, void *data, u32 len )
{
    int res = 0;
    switch (q->buf_type)
    {
    case Q_BUF8: 
        res = spsc_enqueue_u8(q, (u8*)data, len);
        break;
    case Q_BUF16:
        res = spsc_enqueue_u16(q, (u16*)data, len);
        break;
    case Q_BUF32:
        res = spsc_enqueue_u32(q, (u32*)data, len);
        break;
    case Q_BUF64:
        res = spsc_enqueue_u64(q, (u64*)data, len);
        break;
    case Q_BUFP:
        res = spsc_enqueue_ptr(q, (void**)data, len);
        break;    
    default:
        res = 0;
        break;
    }
    
    return res;
}

CST_ALWAYS_INLINE spsc_dequeue_function(u8, spsc_dequeue_u8)
CST_ALWAYS_INLINE spsc_dequeue_function(u16, spsc_dequeue_u16)
CST_ALWAYS_INLINE spsc_dequeue_function(u32, spsc_dequeue_u32)
CST_ALWAYS_INLINE spsc_dequeue_function(u64, spsc_dequeue_u64)
CST_ALWAYS_INLINE spsc_dequeue_function(void*, spsc_dequeue_ptr)

int spsc_dequeue( spsc_queue_t *q, void *data, u32 len )
{
    int res = 0;
    switch (q->buf_type)
    {
    case Q_BUF8: 
        res = spsc_dequeue_u8(q, (u8*)data, len);
        break;
    case Q_BUF16:
        res = spsc_dequeue_u16(q, (u16*)data, len);
        break;
    case Q_BUF32:
       res = spsc_dequeue_u32(q, (u32*)data, len);
        break;
    case Q_BUF64:
        res = spsc_dequeue_u64(q, (u64*)data, len);
        break;
    case Q_BUFP:
        res = spsc_dequeue_ptr(q, (void**)data, len);
        break;    
    default:
        res = 0;
        break;
    }
    
    return res;
}

mpmc_queue_t *mpmc_queue_new( u32 n_queue, u32 buf_size_per_q, q_buf_type_t buf_type )
{
    mpmc_queue_t *res = NULL;
    defer_stack_dynamic(n_queue + 2, on_failure, NULL);
    res = calloc( 1, sizeof( mpmc_queue_t ) );
    if ( defer_add(res, &free, on_failure) )
    {
        perror( "calloc" );
        goto failure;
    }

    res->n_queue = n_queue;
    res->queues = calloc( n_queue, sizeof( spsc_queue_t * ) );
    if ( defer_add(res->queues, &free, on_failure) )
    {
        perror( "calloc" );
        goto failure;
    }

    for ( int i = 0; i < n_queue; i++ )
    {
        res->queues[i] = spsc_queue_new( buf_size_per_q, buf_type );
        if(defer_add(res->queues[i], &spsc_queue_clean, on_failure))
        {
            perror("calloc");
            goto failure;
        }
    }
    res->buf_type = buf_type;
    return res;
failure:
    defer_release(on_failure);
    return NULL;
}

void mpmc_queue_clean( mpmc_queue_t *q )
{
    for ( int i = 0; i < q->n_queue; i++ )
    {
        spsc_queue_clean( q->queues[i] );
    }
    free( q->queues );
    free( q );
}

int mpmc_prod( mpmc_queue_t *q, u32 *data, u32 len )
{
    int res = 0, try = 0;
    u16 p_prod = 0;
    spsc_queue_t *scsp_q;
    bool expected;
    while ( try < q->n_queue && res < len )
    {
        try++;
        scsp_q = q->queues[__atomic_fetch_add( &q->p_prod, 1, __ATOMIC_SEQ_CST ) % q->n_queue];
        expected = false;
        if ( __atomic_compare_exchange_n( &scsp_q->mutex, &expected, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST ) )
        {
            res += spsc_enqueue( scsp_q, &data[res], len - res );
            __atomic_store_n( &scsp_q->mutex, false, __ATOMIC_SEQ_CST );
        }
    }
    return res;
}

int mpmc_cons( mpmc_queue_t *q, u32 *data, u32 len )
{
    int res = 0, try = 0;
    u16 p_cons;
    spsc_queue_t *scsp_q;
    bool expected;
    while ( try < q->n_queue && res < len )
    {
        try++;
        scsp_q = q->queues[__atomic_fetch_add( &q->p_cons, 1, __ATOMIC_SEQ_CST ) % q->n_queue];
        expected = false;
        if ( __atomic_compare_exchange_n( &scsp_q->mutex, &expected, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST ) )
        {
            res += spsc_dequeue( scsp_q, &data[res], len - res );
            __atomic_store_n( &scsp_q->mutex, false, __ATOMIC_SEQ_CST );
        }
    }
    return res;
}