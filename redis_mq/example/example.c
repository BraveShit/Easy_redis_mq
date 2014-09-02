#include "redismq.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include <pthread.h>
#include <unistd.h> /*sleep*/

#define test_log printf

#define MODULE "MEDIA"
static struct redis_mq * module_mq = NULL;
char module[256] = "MEDIA";
char msgtype[256] = "msg_type1";


char *test_send_msg(char *msg)
{
        //int i;
        char send_buf[256] = {0};
        snprintf(send_buf, sizeof(send_buf), "%s", msg);
        //for(i=0; i<10; i++){
        while(1){
                if(redis_mq_send_msg(module_mq, msgtype, send_buf)){
                        printf("[%s] sending <%s> FAILURED!\n", module, send_buf);
                }else{
                        test_log("\E[32m---{%s} sending\E[0m--->(%s)------->\n", module, send_buf);
                }
                sleep(3);
        }
}

void *test_recv_msg(void *null)
{
    char buffer[256] = {0};
    int length = sizeof(buffer);
    int result = -1;

    while(1){
            //result = redis_mq_get_msg(module_mq, buffer, length);
            result = redis_mq_get_msg_timeout(module_mq, buffer, length, 1); /* we set timeout getting msg */
            switch(result){
                    case REDIS_MQ_SUCC:
                            test_log("\E[36m>>>>>>>>{%s} received>>>>>>\E[0m(%s)------->\n", module, buffer);
                            break;
                    case REDIS_MQ_NODATA:
                            test_log("\E[36m>>>>>>>>{%s} received>>>>>>\E[0m--\033[0;31mNo Data\033[0;0m----->\n", module);
                            break;
                    case REDIS_MQ_ERR:
                            test_log("\E[36m>>>>>>>>{%s} received>>>>>>\E[0m--\033[0;31mFAILURED\033[0;0m----->\n", module);
                            break;
                    default:
                            test_log("\E[36m>>>>>>>>{%s} received>>>>>>\E[0m--\033[0;31mShouldn't happen!\033[0;0m----->\n", module);
                            break;

            }
    }
}


int main(int argc, char *argv[])
{

        char buf[256] = {0};
        char module[255] = {0};
        char msgtype[256] =  {0};
        //int i = 0;

        pthread_attr_t pta;
        pthread_t recv_thread_id;

        snprintf(module, sizeof(module), MODULE);
        snprintf(msgtype, sizeof(msgtype), "msg_type1");

        
        module_mq = redis_mq_new(module);
        if(!module_mq){
                printf("Canont create mq! \n");
                return 0;
        }

	pthread_attr_init(&pta);
	pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_JOINABLE);


        test_log("Creating thread to recv msg...\n");
        if(pthread_create(&recv_thread_id, &pta, test_recv_msg, NULL) < 0){
                test_log("Could not start new thread:<send_msg>\n");
        }

        snprintf(buf, sizeof(buf), "(%s):msg:example", msgtype);
        test_send_msg(buf);

	test_log("Waiting recv thread\n");
        pthread_join(recv_thread_id, NULL);


	pthread_attr_destroy(&pta);

        redis_mq_destroy(module_mq);
        return 0;
}
