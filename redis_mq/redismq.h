/*********************************************
**File					: redis_mq.h
**Author				: Shi Xuyong
**Contact				:shixuyong@sict.ac.cn
**Created time			:2014/2/25
**Brief					:message queue

**Development notes		:
**		note 2014/2/25	:begin to write the functions
**		note 2014/2/27	:rename the structs and functions,add notes	
**		note 2014/3/20	:remove the global var redis_mq * m_mq
**********************************************/

#ifndef REDIS_MQ
#define REDIS_MQ

/**********INCLUDE***********/
#include "hiredis.h"
#include <pthread.h>

#define DEFAULT_MAXSIZE 1024

#define REDIS_MQ_SUCC 0
#define REDIS_MQ_NODATA  -1
#define REDIS_MQ_ERR -2

enum message_part{
	MSG_SENDER = 0,
	MSG_RECEIVER = 1,
	MSG_TYPE = 2,
	MSG_BODY = 3,
};

/************STRUCT*****************/
/* redis connection */
struct __connectRedis{
	redisContext *cxt;
	redisReply *reply;
	char ip[64];				/*redis server IP */
	unsigned int port;                      /*redis server Port*/
	char dbname[64];
	pthread_mutex_t lock;
} ;


/*
module's struct.each module have unique name. read from the configure file
*/
struct mq_client{
	char name[64];
	int maxsize;
	int flag; //val =1 when queue is over size
	struct mq_client *next;
};

/*
message type's struct, with unique name, read from the configure file
*/
struct mq_message{
	struct mq_client *c_head;
	char name[64];
	struct mq_message*next;  
};

struct redis_mq{
	struct __connectRedis *con_redis_send;//redis connection to send message
	struct __connectRedis *con_redis_recv;//redis connection to recive message 
	struct mq_client *m_c;  //module which own this redis_mq
	struct mq_message *m_head; //context entrance
	struct mq_client *client_list;
};

/**********PROTOTYPES**************/

/**
**Function			:redis_mq_new
**brief				:create redis_mq
					read confirgure file,create context,two redis connection,
**params name		:module name, must exist in configure file,
**return			: val = NULL fialed
				val = m_mq success
**/
struct redis_mq* redis_mq_new(const char *name);

/*
**Function			:redis_mq_destroy
**brief				:destory redis_mq, release the memory
					when the module unload,should destory the redis_mq object.
**params m_mq		:
**return				: 0
**/

int redis_mq_destroy(struct redis_mq *m_mq);


/**
**Function			:redis_mq_send_msg(_direct)
**brief				:interface to send message for users
					if the list is over maxsize,we will drop the oldlest message
**params m_mq		:user's m_mq pointer
**params message_recver      :the reciver of this message
**params message_type		:message type,must exist in the configure file [message]section
**params message_body	:user's data that will be sended
**return				:val =-1, error happened
					val = 0,send message successful
**/
int redis_mq_send_msg_direct(struct redis_mq *m_mq,const char *message_recver,const char *message_type, const char * message_body);

int redis_mq_send_msg(struct redis_mq *m_mq,const char *message_type, const char * message_body);


/**
**Function			:redis_mq_get_msg
**brief				:interface to get message 
					use block way to get message from redis list, if the message list has no message now,
					this function will block until new message is pushed in. 
**params m_mq		:user;s m_mq
**params buffer		:buffer to store the message get from mq.
					user should allocate enough memory for this buffer.
**params length 		:the buffer length

**params timeout		:if the list is block,how long we wait for the next message
					0,block no matter how long
**return				REDIS_MQ_ERR, error happened
					REDIS_MQ_SUCC,  got message 
                                        REDIS_MQ_NODATA, no data found
**/
int redis_mq_get_msg_timeout(struct redis_mq *m_mq, char *buffer,int length, int timeout);

int redis_mq_get_msg(struct redis_mq *m_mq,char *buffer,int length);

/**
**Function		:redis_mq_message_
**brief			:get different part of a message
**/
int redis_mq_message_sender(char *message,char *buff,int len);
int redis_mq_message_receiver(char *message,char *buff,int len);
int redis_mq_message_body(char *message,char *buff,int len);
int redis_mq_message_type(char *message,char *buff,int len);




#endif /*REDIS_MQ*/

#ifndef __BUS_LOG__
#define __BUS_LOG__

#ifdef BUSLOG_DEBUG
#undef BUSLOG_DEBUG
#endif
#define __BUSLOG_DEBUG	7
#define BUSLOG_DEBUG	__BUSLOG_DEBUG

#ifdef BUSLOG_INFO
#undef BUSLOG_INFO
#endif
#define __BUSLOG_INFO	6
#define BUSLOG_INFO	__BUSLOG_INFO

#ifdef BUSLOG_NOTICE
#undef BUSLOG_NOTICE
#endif
#define __BUSLOG_NOTICE	5
#define BUSLOG_NOTICE	__BUSLOG_NOTICE

#ifdef BUSLOG_WARNING
#undef BUSLOG_WARNING
#endif
#define __BUSLOG_WARNING	4
#define BUSLOG_WARNING	__BUSLOG_WARNING

#ifdef BUSLOG_ERROR
/* bus_log api
 *                move to redis_mq.h by wangyahui
 */
#undef BUSLOG_ERROR
#endif
#define __BUSLOG_ERROR	3
#define BUSLOG_ERROR	__BUSLOG_ERROR

#define MAXSIZEBUFFER 1024   
#define MSGTOBUSLOGGERTYPE  "msg2log"

