/********************************************************
 * Filename: core/comm.c
 *  
 * Author: jtlim, RTOSLab. SNU.
 * 
 * Description: message queue management. 
 ********************************************************/
#include <core/eos.h>

void eos_init_mqueue(eos_mqueue_t *mq, void *queue_start, int16u_t queue_size, int8u_t msg_size, int8u_t queue_type) {
    mq -> queue_size = queue_size;
    mq -> msg_size = msg_size;
    mq -> queue_start = queue_start;
    mq -> queue_type = queue_type;
    eos_init_semaphore(&(mq -> putsem), (int32u_t)queue_size, queue_type);
    eos_init_semaphore(&(mq -> getsem), 0, queue_type);
    mq -> front = NULL;
    mq -> rear = NULL;
    // TODO: Max queue length도 고려해야. last등
}

int8u_t eos_send_message(eos_mqueue_t *mq, void *message, int32s_t timeout) {
    PRINT("eos_send_message\n");
    if (eos_acquire_semaphore(&mq -> putsem, timeout)){ 
        // semaphore 획득 성공
        for(int8u_t i = 0; i < mq -> msg_size; i++){
            if (!(mq -> front)){ // Null일 경우, queue의 첫 값으로 초기화
                PRINT("Make rear, front UN Null\n");
                mq -> front = mq -> queue_start;
                mq -> rear = (int8u_t *)(mq -> front) - 1;
            }
            *(int8u_t *)++(mq -> rear) = ((int8u_t *)message)[i];
        }
        PRINT("Message is - %s\n", (int8u_t *)mq->front)
        PRINT("release\n");
        eos_release_semaphore(&(mq -> getsem));
        PRINT("done release\n");
    } else { 
        // semaphore 획득 실패
    }
    return 0;
}

int8u_t eos_receive_message(eos_mqueue_t *mq, void *message, int32s_t timeout) {
    // PRINT("eos_receive_message\n");
    if (eos_acquire_semaphore(&mq -> getsem, timeout)){
        // semaphore 획득 성공
        for(int8u_t i = 0; i < mq -> msg_size; i++){
            ((int8u_t *)message)[i] = *((int8u_t* )(mq -> front)++);
            if (mq -> rear == (int8u_t *)(mq -> queue_start) - 1){ // queue에 데이터가 없을 경우, Null로
                PRINT("Make rear, front NULL");
                mq -> rear = mq -> front = NULL;
            }
        }
        // PRINT("release\n");
        eos_release_semaphore(&(mq ->putsem));
        // PRINT("done release\n");
    } else {
        // semaphore 획득 실패
    }
    return 0;
}
