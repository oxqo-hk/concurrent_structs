#ifndef CST_MPMC_Q_H
#define CST_MPMC_Q_H
#include <stdbool.h>
#include "common.h"

typedef enum {
    Q_BUF8,
    Q_BUF16,
    Q_BUF32,
    Q_BUF64,
    Q_BUFP,
}q_buf_type_t;

typedef struct spsc_queue {
    u32 consumer;
    u32 producer;
    void *buffer;
    q_buf_type_t buf_type;
    u32 buffer_size;
    bool mutex;
}spsc_queue_t;

typedef struct mpmc_queue {
    u16 n_queue;
    u16 p_prod;
    u16 p_cons;
    q_buf_type_t buf_type;
    spsc_queue_t **queues;
}mpmc_queue_t;

spsc_queue_t *spsc_queue_new( u32 buf_size, q_buf_type_t buf_type );
void spsc_queue_clean( spsc_queue_t *q );
int spsc_enqueue( spsc_queue_t *q, void *data, u32 len );
int spsc_dequeue( spsc_queue_t *q, void *data, u32 len );

mpmc_queue_t *mpmc_queue_new( u32 n_queue, u32 buf_size_per_q, q_buf_type_t buf_type );
void mpmc_queue_clean( mpmc_queue_t *q );
int mpmc_prod( mpmc_queue_t *q, u32 *data, u32 len );
int mpmc_cons( mpmc_queue_t *q, u32 *data, u32 len );

#endif