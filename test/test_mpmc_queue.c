#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <memory.h>
#include <errno.h>

#include "common.h"
#include "mpmc_queue.h"
#include "test_mpmc_queue.h"

#define TEST_CASE_SIZE ((u32)(65535 * 16))
#define TEST_PRODUCER_CNT 8
#define TEST_CONSUMER_CNT 8
#define TEST_BATCH_SIZE_MAX 8

typedef struct {
    mpmc_queue_t *q;
    u32 *buffer;
    u32 len;
    u32 batch_max;
}thr_arg_t;

int u32_compare( const void *a, const void *b )
{
    return ( *( int * )a ) - ( *( int * )b );
}

void *producer_thread( void *arg )
{
    mpmc_queue_t *q = ( ( thr_arg_t * )arg )->q;
    u32 *buffer = ( ( thr_arg_t * )arg )->buffer;
    u32 len = ( ( thr_arg_t * )arg )->len;
    u32 index = 0;
    u32 batch_max = ( ( thr_arg_t * )arg )->batch_max;
    u32 batch;
    while ( index < len )
    {
        batch = ( ( len - index ) > batch_max ? batch_max : ( len - index ) );
        index += mpmc_prod( q, &buffer[index], batch );
    }
    return NULL;
}

void *consumer_thread( void *arg )
{
    mpmc_queue_t *q = ( ( thr_arg_t * )arg )->q;
    u32 *buffer = ( ( thr_arg_t * )arg )->buffer;
    u32 len = ( ( thr_arg_t * )arg )->len;
    u32 index = 0;
    u32 batch_max = ( ( thr_arg_t * )arg )->batch_max;
    u32 batch;
    while ( index < len )
    {
        batch = ( ( len - index ) > batch_max? batch_max : ( len - index ) );
        index += mpmc_cons( q, &buffer[index], batch );
    }
    return NULL;
}

test_result test_mpmc_integrity()
{
    u32 *test_case, *sorted, *result;
    pthread_t *pthread;
    thr_arg_t *args;
    mpmc_queue_t *mpmc_q;
    void *thr_return[256];
    defer_stack(16, on_cleanup);
    int err;

    test_start();
    srand( time( NULL ) );
    test_case = calloc( TEST_CASE_SIZE, sizeof( u32 ) );

    if ( defer_add(test_case, &free, on_cleanup) )
    {
        perror( "calloc" );
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }

    sorted = calloc( TEST_CASE_SIZE, sizeof( u32 ) );
    if ( defer_add(sorted, &free, on_cleanup) )
    {
        perror( "calloc" );
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }

    result = calloc( TEST_CASE_SIZE, sizeof( u32 ) );
    if ( defer_add(result, &free, on_cleanup))
    {
        perror( "calloc" );
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }

    //gernerate test cases.
    for ( int i = 0; i < TEST_CASE_SIZE; i++ )
    {
        test_case[i] = ( u32 )rand();
    }

    //copy and sort data.
    memcpy( sorted, test_case, sizeof( u32 ) * TEST_CASE_SIZE );
    qsort( sorted, TEST_CASE_SIZE, sizeof( u32 ), u32_compare );

    pthread = calloc( TEST_PRODUCER_CNT + TEST_CONSUMER_CNT, sizeof( pthread_t ) );
    if ( defer_add(pthread, &free, on_cleanup) )
    {
        perror( "calloc" );
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }

    args = calloc( TEST_PRODUCER_CNT + TEST_CONSUMER_CNT, sizeof( thr_arg_t ) );
    if ( defer_add(args, &free, on_cleanup) )
    {
        perror( "calloc" );
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }

    mpmc_q = mpmc_queue_new( 8, 8192, Q_BUF32 );
    if ( defer_add(mpmc_q, &mpmc_queue_clean, on_cleanup))
    {
        err = TEST_FAIL_SETUP;
        goto cleanup;
    }


    u32 tc_per_prod = TEST_CASE_SIZE / TEST_PRODUCER_CNT;
    u32 prod_index = 0;
    for ( int i = 0; i < TEST_PRODUCER_CNT; i++ )
    {
        args[i].buffer = &test_case[prod_index];
        args[i].len = tc_per_prod;
        args[i].q = mpmc_q;
        args[i].batch_max = TEST_BATCH_SIZE_MAX;
        if ( i == ( TEST_PRODUCER_CNT - 1 ) )
        {
            args[i].len += TEST_CASE_SIZE % TEST_PRODUCER_CNT;
        }
        pthread_create( &pthread[i], NULL, producer_thread, &args[i] );
        printf( "Producer thread %d for result[%u] - result[%u]\n", i, prod_index, prod_index + args[i].len );
        prod_index += tc_per_prod;
    }

    u32 tc_per_cons = TEST_CASE_SIZE / TEST_CONSUMER_CNT;
    u32 cons_index = 0;
    for ( int i = TEST_PRODUCER_CNT; i < TEST_PRODUCER_CNT + TEST_CONSUMER_CNT; i++ )
    {
        args[i].buffer = &result[cons_index];
        args[i].len = tc_per_cons;
        args[i].q = mpmc_q;
        args[i].batch_max = TEST_BATCH_SIZE_MAX;
        if ( i == ( TEST_PRODUCER_CNT + TEST_CONSUMER_CNT - 1 ) )
        {
            args[i].len += TEST_CASE_SIZE % TEST_CONSUMER_CNT;
        }
        pthread_create( &pthread[i], NULL, consumer_thread, &args[i] );
        printf( "Consumer thread %d for result[%u] - result[%u]\n", i, cons_index, cons_index + args[i].len );
        cons_index += tc_per_cons;
    }

    for ( int i = 0; i < TEST_PRODUCER_CNT + TEST_CONSUMER_CNT; i++ )
    {
        pthread_join( pthread[i], thr_return );
    }

    printf( "sorting results.\n" );

    qsort( result, TEST_CASE_SIZE, sizeof( u32 ), u32_compare );
    u32 wrong_cnt = 0;
    for ( int i = 0; i < TEST_CASE_SIZE; i++ )
    {
        if ( sorted[i] != result[i] )
        {
            printf( "wrong in index: %d %u:%u\n", i, sorted[i], result[i] );
            wrong_cnt++;
        }
    }
    printf( "total error: %d/%u %.2f%%\n", wrong_cnt, TEST_CASE_SIZE, (wrong_cnt * 100.0)/TEST_CASE_SIZE );
    if(wrong_cnt != 0)
    {
        err = TEST_FAIL_INTEGRITY;
    }
cleanup:
    test_end();
    defer_release(on_cleanup);
    return 0;

}