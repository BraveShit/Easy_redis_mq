/*********************************************
**File					: redismq.c
**Author				: Shi Xuyong
**Contact				:shixuyong@sict.ac.cn
**Created time			:2014/2/25
**Brief					:message queue

**Development notes		:
**		note 2014/2/25	:begin to write the functions
**		note 2014/2/27	:rename the structs and functions,add notes		
**********************************************/



/*********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "redismq.h"
#include "readconf.h"

#define CONFIG_FILE "/etc/redis_mq.conf"


/*************prototypes***********/
static int strlen_zero(const char *s);
static int destroy_context(struct mq_message **m_head);
static struct mq_message * create_context(struct _CFG *cfg);
static int parse_clients(struct _CFG *cfg,char *clients,struct mq_client **c_list);
static struct mq_client * new_mq_client();
static struct mq_client * create_client_list(struct _CFG *cfg);
static int redis_mq_send_msg_multi(struct redis_mq *m_mq,const char *message_recver,const char *message_type, const char * message_body,int direct);
static int redis_mq_message_part(const char * message,char * buff,int len,int part);


static struct __connectRedis *redis_new_connection(const char* redis_server,const unsigned redis_port,const char *dbname);
static int redis_connect(struct __connectRedis * con_redis);
static int redis_connection_destroy(struct __connectRedis * con_redis);
static int redis_reconnect(struct __connectRedis * con_redis);
static int select_redis_db(struct __connectRedis *con_redis);
static int redis_detect_disconnect(struct __connectRedis * con_redis);


/***************interface******************/
struct redis_mq * redis_mq_new(const char *name)
{
        struct redis_mq *m_mq = NULL;
        struct _CFG *cfg_info = NULL;
        char *dbhost, *dbport,*dbname,*mb_name,*mb_maxsize;
        struct __connectRedis *con_redis1 = NULL;
        struct __connectRedis *con_redis2 = NULL;
        struct mq_message *m_head = NULL;

		/*if the m_mq is existed,destroy it,then recreate it*/
//		if(m_mq)
//			redis_mq_destroy();

        if(strlen_zero(name)){
                printf("name is empty!\n");
                return NULL;
        }

        m_mq = (struct redis_mq *) malloc(sizeof(struct redis_mq));
        if(!m_mq){
                printf("cant alloc memery for mq\n");
                return NULL;
        }
        memset(m_mq,0,sizeof(struct redis_mq));
        if(!(m_mq->m_c=(struct mq_client *)new_mq_client())){
                printf("cant create mq_client \n");
                return NULL;
        }
        snprintf(m_mq->m_c->name,sizeof(m_mq->m_c->name), "%s", name);



        /* read configure */
        cfg_info = cfg_read(CONFIG_FILE);
        if (!cfg_info)
        {
                printf("read configure file(%s) failed\n", CONFIG_FILE);
                goto out;
        }
		if(!(m_mq->client_list = create_client_list(cfg_info))){
			printf("has no client defined in the configure file \n");
			goto out;
		}

		/*according configure information to create the send and recv redis connection*/
        if (!(dbhost = cfg_get_val(cfg_info, "general", "dbhost")) ||
                        !(dbport = cfg_get_val(cfg_info, "general", "dbport"))||
                        !(dbname = cfg_get_val(cfg_info, "general", "dbname"))
           )
        {
                printf("check the config file[general]part,wrong conf \n");
                goto out;
        }

        con_redis1 = redis_new_connection(dbhost, atoi(dbport),dbname);
        if(con_redis1 == NULL){
                printf("cant create new struct __connectRedis\n");
                goto out;
        }

        if (redis_connect(con_redis1) == REDIS_ERR) {
                printf("cant connect redis server\n");
                redis_connection_destroy(con_redis1);
                goto out;
        }

        con_redis2 = redis_new_connection(dbhost, atoi(dbport),dbname);
        if(con_redis2 == NULL){
                printf("cant create new struct __connectRedis\n");
                goto out;
        }

        if (redis_connect(con_redis2) == REDIS_ERR) {
                printf("cant connect redis server\n");
                redis_connection_destroy(con_redis2);
                goto out;
        }
        //printf("connect redis sever success\n");
        m_mq->con_redis_send = con_redis1;
        m_mq->con_redis_recv = con_redis2;
		mb_maxsize = cfg_get_val(cfg_info, name, "maxsize");
			if(!mb_maxsize)
				m_mq->m_c->maxsize = DEFAULT_MAXSIZE;
			else
		        m_mq->m_c->maxsize=atoi(mb_maxsize);

		/*create context ,get the context head message pointer*/
        m_head = create_context(cfg_info);
        if(!m_head){
                printf("cant create context,check configure file,some modules are not defined\n");
                goto out;
        }
        m_mq->m_head = m_head;

        cfg_clean(cfg_info);
        return m_mq;
out :
		cfg_clean(cfg_info);
        redis_mq_destroy(m_mq);
        return NULL;
}


int redis_mq_destroy(struct redis_mq *m_mq)
{
		struct mq_client *tmp = NULL;
		struct mq_client *next = NULL;
        if(!m_mq)
                return 0;
		tmp = m_mq->client_list;
		while(tmp){
			next = tmp->next;
			free(tmp);
			tmp = next;
		}
		/*release m_head memory*/
        destroy_context(&(m_mq->m_head));
		if(m_mq->m_head)
			free(m_mq->m_head);
        m_mq->m_head=NULL;

        /*release m_c*/
        if(m_mq->m_c){
                free(m_mq->m_c);
                m_mq->m_c=NULL;
        }

		/*destroy redis connection*/
        if(m_mq->con_redis_send){
                redis_connection_destroy(m_mq->con_redis_send);
                m_mq->con_redis_send=NULL;
        }
        if(m_mq->con_redis_recv){
                redis_connection_destroy(m_mq->con_redis_recv);
                m_mq->con_redis_recv=NULL;
        }
        if(m_mq){
	        free(m_mq);
                m_mq = NULL;
        }
        return 0;
}

int redis_mq_send_msg_direct(struct redis_mq *m_mq,const char *message_recver,const char *message_type, const char * message_body)
{
	return redis_mq_send_msg_multi(m_mq,message_recver,message_type,message_body,1);
}
int redis_mq_send_msg(struct redis_mq *m_mq,const char *message_type, const char * message_body)
{
	return redis_mq_send_msg_multi(m_mq,NULL,message_type,message_body,0);
}

/**
**Function			:redis_mq_send_msg
**brief 			:interface to send message for users
						if the list is over maxsize,we will drop the oldlest message
**params m_mq		:user's m_mq pointer
**params message_recver 	 :the reciver of this message
**params message_type		:message type,must exist in the configure file [message]section
**params message_body	:user's data that will be sended
**params direct :send the message directly by reciver.	
**return				:val =-2, error happened
					val = 0,send message successful
**/

static int redis_mq_send_msg_multi(struct redis_mq *m_mq,const char *message_recver,const char *message_type, const char * message_body,int direct)
{
        struct mq_client *c_tmp = NULL;
		struct mq_client *recv_client = NULL;
        struct mq_message *m_tmp = NULL;
        char cmd[256] = {0};
		 char message[4096] = "";

        if(!m_mq)
                return REDIS_MQ_ERR;
        if(REDIS_ERR == redis_detect_disconnect(m_mq->con_redis_send)){
                printf("connection with redis has problem\n");
                return REDIS_MQ_ERR;
        }
		if(strlen_zero(message_body)||strlen_zero(message_type)){
			printf("message body cant be empty\n");
			return REDIS_MQ_ERR;
		}
		
		//send message directly to recver
		if(direct == 1){
			if(strlen_zero(message_recver))
				goto out;
			struct mq_client *iterator = m_mq->client_list;
			while(iterator){
				if(!strcasecmp(iterator->name,message_recver)){
					recv_client = new_mq_client();
					snprintf(recv_client->name,sizeof(recv_client->name),"%s",message_recver);
					recv_client->maxsize = iterator->maxsize;
					recv_client->next = NULL;
					break;
				}
				iterator = iterator->next;
			}
			if(!recv_client){
				printf("no such client named %s\n",message_recver);
				goto error;
			}
			c_tmp = recv_client;
			goto send_message;
		}



		//user message_type ,send to recver according configure file
		if(strlen_zero(message_type)){
			printf("message type or message cant be NULL\n");
			goto error;
		}

		/*find the message object by message type, if cant find, return error*/
        m_tmp=m_mq->m_head;
        while(m_tmp){
                if(0 == strcasecmp(m_tmp->name,message_type))
                        break;
                m_tmp=m_tmp->next;
        }
        if(!m_tmp){
                printf("undefined message type %s in configure \n",message_type);
               goto error;
        }
  
        if(!(c_tmp=m_tmp->c_head)){
                printf("no client subscribe %s message",message_type);
                goto error;
        }

		/*according the clients list ,send message for all those client with atomic operation */
		c_tmp = m_tmp->c_head;


send_message:		
        pthread_mutex_lock(&m_mq->con_redis_send->lock);
        m_mq->con_redis_send->reply = (redisReply *)redisCommand(m_mq->con_redis_send->cxt,"MULTI");
		if(NULL == m_mq->con_redis_send->reply){
			goto out;
		}
        if( !(m_mq->con_redis_send->reply->type == REDIS_REPLY_STATUS && strcasecmp(m_mq->con_redis_send->reply->str,"OK")==0))  
        {  
                freeReplyObject(m_mq->con_redis_send->reply);
                m_mq->con_redis_send->reply=NULL;
                goto  out;  
        } 
        freeReplyObject(m_mq->con_redis_send->reply);
		m_mq->con_redis_send->reply=NULL;

        while(c_tmp){
                //snprintf(cmd, sizeof(cmd), "LPUSH %s %s", c_tmp->name, message);
                snprintf(message,sizeof(message),"from:%s\r\nto:%s\r\ntype:%s\r\nbody:%s",m_mq->m_c->name,c_tmp->name,message_type == NULL?"":message_type,message_body);
                m_mq->con_redis_send->reply = (redisReply *)redisCommand(m_mq->con_redis_send->cxt,"LPUSH %s %s",c_tmp->name,message);
					if(NULL == m_mq->con_redis_send->reply){
						goto out;
					}
                freeReplyObject(m_mq->con_redis_send->reply); 
                m_mq->con_redis_send->reply=NULL;
					m_mq->con_redis_send->reply = (redisReply *)redisCommand(m_mq->con_redis_send->cxt,"LTRIM %s %d %d",c_tmp->name,0,c_tmp->maxsize-1);
					if(NULL == m_mq->con_redis_send->reply){
						goto out;
					}
                freeReplyObject(m_mq->con_redis_send->reply); 
                m_mq->con_redis_send->reply=NULL;
                c_tmp=c_tmp->next;
        }

        m_mq->con_redis_send->reply = (redisReply *)redisCommand(m_mq->con_redis_send->cxt,"EXEC");
        if(NULL == m_mq->con_redis_send->reply || m_mq->con_redis_send->reply->type != REDIS_REPLY_ARRAY){ 
                if(m_mq->con_redis_send->reply){
					freeReplyObject(m_mq->con_redis_send->reply);  
			 		m_mq->con_redis_send->reply=NULL;
					}
                goto  out;  

        }
		if(m_mq->con_redis_send->reply){
			freeReplyObject(m_mq->con_redis_send->reply);  
			 m_mq->con_redis_send->reply=NULL;
			}
		
        pthread_mutex_unlock(&m_mq->con_redis_send->lock);
		if(direct == 1 && recv_client)
			free(recv_client);
        return REDIS_MQ_SUCC;
out :
        pthread_mutex_unlock(&m_mq->con_redis_send->lock);
error:
		if(direct == 1 && recv_client)
			free(recv_client);
        return REDIS_MQ_ERR;
}

/* \brief blocking get_msg
 *
 * \retval REDIS_MQ_SUCC, for success
 * \retval REDIS_MQ_NODATA for no data
 * \retval REDIS_MQ_ERR, for error
 */

int redis_mq_get_msg(struct redis_mq *m_mq,char *buffer,int length)
{
        return redis_mq_get_msg_timeout(m_mq, buffer, length, 0);
}

/* \brief blocking with timeout get_msg
 *
 * \retval REDIS_MQ_SUCC, for success
 * \retval REDIS_MQ_NODATA for no data
 * \retval REDIS_MQ_ERR, for error
 */
int redis_mq_get_msg_timeout(struct redis_mq *m_mq, char *buffer,int length, int timeout)
{
        int ret = REDIS_MQ_ERR;
        //int i;
        char cmd[256] = {0};

        if(!m_mq)
                return ret;
        if(!buffer || length < 0){
                printf("Buffer is not avaliable!\n");
                return REDIS_MQ_ERR;
        }

        if(timeout <= 0)
                timeout= 0;

        if(REDIS_ERR == redis_detect_disconnect(m_mq->con_redis_recv))
                return ret;
        pthread_mutex_lock(&m_mq->con_redis_recv->lock);

        snprintf(cmd, sizeof(cmd), "BRPOP %s %d", m_mq->m_c->name, timeout);
        //printf("\E[36m>>>>>>\E[0m>>before [%s]>>>>>>\n", cmd);
        m_mq->con_redis_recv->reply = redisCommand(m_mq->con_redis_recv->cxt, cmd);
        //printf("\E[36m>>>>>>\E[0m>>after exec [%s]>>>>>>\n", cmd);

        if(m_mq->con_redis_recv->reply != NULL){
                switch(m_mq->con_redis_recv->reply->type ){
                        case REDIS_REPLY_STRING:
                                /* RPOP cmd's return value: just <value>*/
                                /* If the cmd is BRPOP  above, shouldn't come here
                                 */
                                if(!strlen_zero(m_mq->con_redis_recv->reply->str)){
                                        memset(buffer, 0, length);
                                        strncpy(buffer, m_mq->con_redis_recv->reply->str, length);
                                        //printf("\E[36m>>>>>>>>got string(%s)>>>>>\E[0m\n", m_mq->con_redis_recv->reply->str);
                                        ret = REDIS_MQ_SUCC;

                                }
                                break;
                        case REDIS_REPLY_ARRAY:

                                /* XXX:In this case, BRPOP return ARRAY:<key>,<value>
                                 *  elements == 2
                                 *  eg:
                                 *      BRPOP list  0
                                 *      1) "list" --> key
                                 *      2) "a"  ---> the value we need
                                 */
                                //printf("\E[36m>>>>>>>>got ARRAY <%d>'s elements>>>>>>\E[0m\n", m_mq->con_redis_recv->reply->elements);
                                if(m_mq->con_redis_recv->reply->elements == 2){
                                        memset(buffer, 0, length);
                                        snprintf(buffer, length, "%s", m_mq->con_redis_recv->reply->element[1]->str);
                                        //printf("\E[36m>>>>>>>>got array(%s)>>>>>\E[0m\n", buffer);
                                        ret = REDIS_MQ_SUCC;
                                }
                                break;
                        case REDIS_REPLY_NIL:
                                ret = REDIS_MQ_NODATA;
                                break;
                        case REDIS_REPLY_STATUS:
                        case REDIS_REPLY_INTEGER:
                        default:
                                ret = REDIS_MQ_ERR;
                                break;
                }

                if(m_mq->con_redis_recv->reply){//add by wangyahui, 2014-02-26
                        freeReplyObject(m_mq->con_redis_recv->reply);
                        m_mq->con_redis_recv->reply=NULL;
                }
        }

        //printf("\E[36m>>>>>>>\E[0m>after reply [%s]>>>>ret:[%d]>>\n", cmd, ret);

        pthread_mutex_unlock(&m_mq->con_redis_recv->lock);
        return ret;

}

int redis_mq_message_sender(char *message,char *buff,int len)
{
	return redis_mq_message_part(message,buff,len,MSG_SENDER);	
}
int redis_mq_message_receiver(char *message,char *buff,int len)
{
	return redis_mq_message_part(message,buff,len,MSG_RECEIVER);	

}
int redis_mq_message_body(char *message,char *buff,int len)
{
	return redis_mq_message_part(message,buff,len,MSG_BODY);	

}
int redis_mq_message_type(char *message,char *buff,int len)
{
	return redis_mq_message_part(message,buff,len,MSG_TYPE);	

}


/**
**Function redis_mq_message_part
**brief 	:get the specify part of the message
**params	message:
**params	buff:memory to store the specified part
**params	len:buffer length
**params	part:the part of the message
retrun		:val=-2;error
			:val=-1;nodata
			:val=0;success
**/
static int redis_mq_message_part(const char * message,char * buff,int len,int part)
{
///*
	char *sender = NULL;
	char *receiver = NULL;
	char * type = NULL;
	char *body = NULL;
//	*/
	const char *tmp = message;
	int length = 0;

	if(strlen_zero(message)){
		printf("message is null\n");
		return REDIS_MQ_ERR;
	}


	sender = strstr(tmp,"from:");
	if(!sender)
		return REDIS_MQ_ERR;
	receiver = strstr(sender,"to:");
	if(!receiver)
		return REDIS_MQ_ERR;
	type = strstr(receiver,"type:");
	if(!type)
		return REDIS_MQ_ERR;
	body = strstr(receiver,"body:");
	if(!body){
		return REDIS_MQ_ERR;
	}
	switch(part){
	case MSG_SENDER:
//		length = receiver-sender-sizeof("from");
//		snprintf(buff,length>len?len:length,"%s",sender+sizeof("from:")-1);
		sscanf(message,"%*[^:]:%s",buff);
		break;
	case MSG_RECEIVER:
//		length = type-receiver-sizeof("to:")+1;
//		snprintf(buff,length>len?len:length,"%s",receiver+sizeof("to:")-1);
		sscanf(message,"%*s%*[^:]:%s",buff);
		break;
	case MSG_TYPE:
//		length = body-type-sizeof("type:")+1;
//		snprintf(buff,length>len?len:length,"%s",type+sizeof("type:")-1);
		sscanf(message,"%*s%*s%*[^:]:%s",buff);
		break;
	case MSG_BODY:
		length = message-body-1;
		snprintf(buff,len,"%s",body+sizeof("body:")-1);
		break;
	default:
		return REDIS_MQ_ERR;
	}
	return REDIS_MQ_SUCC;
}

/***********************************************/

/**
**Function			:create_context
**brief				:to create the message context,call by redis_mq_new()
					[message]
					message1 => client1,client2
**params cfg		:
**return				:context head message pointer 
**/
static struct mq_message * create_context(struct _CFG *cfg)
{
        struct mq_message * m_head = NULL;
        struct mq_message * m_tmp = NULL;

        struct _KEY *knode = NULL;

        knode = cfg_get_app(cfg,"message");

        while(knode){
                m_tmp=(struct mq_message *)malloc(sizeof(struct mq_message));
                if(!m_tmp)
                        return NULL;
                memset(m_tmp,0,sizeof(struct mq_message));
                snprintf(m_tmp->name,sizeof(m_tmp->name), "%s", knode->name);
                if(parse_clients(cfg,knode->value,&m_tmp->c_head))
                        return NULL;
                if(m_head){
                        m_tmp->next = m_head->next;
                        m_head->next = m_tmp;
                }
                else{
                        m_head = m_tmp;
                        m_head->next = NULL;
                }
                m_tmp = NULL;
                knode = knode->next;
        }
        return m_head;
}

/**
**Function			:destroy_context
**brief				:to destroy the message context
					call by redis_mq_destroy()
**params cfg		:
**return				:context head message pointer 
**/

static int destroy_context(struct mq_message **m_head)
{
        struct mq_client *c_tmp,*c_tmp1;
        struct mq_message *m_tmp,*m_tmp1;

        if(!*m_head)
                return 0;

        m_tmp = *m_head;

        while(m_tmp){
                m_tmp1 = m_tmp;
                m_tmp = m_tmp->next;

                c_tmp = m_tmp1->c_head;
                while(c_tmp){
                        c_tmp1 = c_tmp;
                        c_tmp = c_tmp->next;
                        free(c_tmp1);
                }
                free(m_tmp1);
				m_tmp1 = NULL;
        }
		*m_head = NULL;
        return 0;
}

/**
**Function			:parse_client
**brief				:parse clients to mq_clients list, call by create_context()
					examples : clients = "client1,client2,client3"
**params cfg		:
**params clients		:
**params c_list		:the list will be created in this function
**return				: val = -1,error
					val = 0,success
**/

static int parse_clients(struct _CFG *cfg,char *clients,struct mq_client **c_list)
{
        char * tmp,*tmp1,client_tmp[4096];
		char * mb_maxsize = NULL;
        struct mq_client *c_tmp = NULL;
   //     tmp = (char *)malloc(strlen(clients) +1);
   //     strcpy(tmp, clients);
   		snprintf(client_tmp,sizeof(client_tmp),"%s",clients);
   		tmp = client_tmp;
        tmp1 = tmp;


        while((tmp = (char *)strchr(tmp, ','))){
                *tmp++ = 0;
                if(!cfg_get_app(cfg,tmp1))
                        return -1;
                c_tmp=new_mq_client();
                if(!c_tmp)
                        return -1;
                snprintf(c_tmp->name,sizeof(c_tmp->name),"%s",tmp1);
				mb_maxsize = cfg_get_val(cfg,tmp1,"maxsize");
				if(!mb_maxsize)
					c_tmp->maxsize = DEFAULT_MAXSIZE;
				else
                c_tmp->maxsize = atoi(mb_maxsize);
                if(*c_list){
                        c_tmp->next=(*c_list)->next;
                        (*c_list)->next=c_tmp;
                }
                else{
                        *c_list=c_tmp;
                        (*c_list)->next=NULL;
                }
                tmp1=tmp;
                c_tmp=NULL;
        }
        if(*tmp1){
                if(!cfg_get_app(cfg,tmp1))
                        return -1;
                c_tmp=new_mq_client();
                if(!c_tmp)
                        return -1;
                snprintf(c_tmp->name,sizeof(c_tmp->name),"%s",tmp1);
                c_tmp->maxsize = atoi(cfg_get_val(cfg,tmp1,"maxsize"));
                if(*c_list){
                        c_tmp->next = (*c_list)->next;
                        (*c_list)->next=c_tmp;
                }
                else{
                        *c_list=c_tmp;
                        (*c_list)->next=NULL;
                }
        }
        return 0;

}

/**
**Function			:new_mq_client
**brief				:allocate memory for new mq_client
**return				:
**/
static struct mq_client * new_mq_client()
{
        struct mq_client *c_tmp=NULL;
        c_tmp = (struct mq_client *)malloc(sizeof(struct mq_client));
        if(!c_tmp)
                return NULL;
        memset(c_tmp,0,sizeof(struct  mq_client));
        c_tmp->flag = 0;
        c_tmp->maxsize = DEFAULT_MAXSIZE;
        c_tmp->next=NULL;
        return c_tmp;
}

int strlen_zero(const char *s)
{
        return (!s || (*s == '\0'));
}

/**
*Function: create_client_list
*brief :put all the client in a list
**/
struct mq_client * create_client_list(struct _CFG * cfg)
{
	struct mq_client *list_head = NULL;
	struct mq_client *client_tmp = NULL;
	struct _CFG *cfg_tmp = cfg;


	while(cfg_tmp){
		if(strcasecmp(cfg_tmp->app,"general") && strcasecmp(cfg_tmp->app,"message")){
			client_tmp = new_mq_client();
			snprintf(client_tmp->name,sizeof(client_tmp->name),"%s",cfg_tmp->app);
			client_tmp->maxsize = atoi(cfg_get_val_with_knode(cfg_tmp->key,"maxsize"));
			if(NULL == list_head){
				list_head = client_tmp;
			}
			else{
				client_tmp->next = list_head;
				list_head = client_tmp;
			}
		}
		cfg_tmp = cfg_tmp->next;
	}
	return list_head;
	
}
#if 0
/***********test*****************/
void show_context(struct mq *m_mq)
{
        struct message *m_tmp;
        struct mq_client *c_tmp;

        printf("************module %s*************\n",m_mq->m_c->name);
        m_tmp=m_mq->m_head;
        while(m_tmp){
                printf("%s => ",m_tmp->name);
                c_tmp=m_tmp->c_head;
                while(c_tmp){
                        printf("%s,",c_tmp->name);
                        c_tmp=c_tmp->next;
                }
                printf("\n");
                m_tmp=m_tmp->next;
        }
        printf("**************end*******************\n");
}
#endif


static struct __connectRedis *redis_new_connection(const char* redis_server,const unsigned redis_port,const char *dbname)
{
        struct __connectRedis *con_redis;

        con_redis = (struct __connectRedis *) malloc(sizeof(struct __connectRedis));
        if(con_redis == NULL)
        {
                printf( "Cannot malloc memory!\n");
                return NULL;
        }
	con_redis->cxt = NULL;
	con_redis->reply = NULL;

	snprintf(con_redis->ip, sizeof(con_redis->ip), "%s", redis_server);
	con_redis->port = redis_port;
	snprintf(con_redis->dbname, sizeof(con_redis->dbname), "%s", dbname);
	pthread_mutex_init(&con_redis->lock,NULL);

	return con_redis;
}

/*
 * \brief reconnect to redis, try THREE times then give up.
 */
static int redis_reconnect(struct __connectRedis * con_redis)
{
	int i;
	if (con_redis == NULL)
		return REDIS_ERR;
	if (con_redis->cxt != NULL)
		return REDIS_OK;
	for (i = 3; i > 0; i--) {
		if (redis_connect(con_redis) == REDIS_OK)
			return REDIS_OK;
	}
	//ast_log(LOG_NOTICE, "give up on reconnecting to redis!\n");
	return REDIS_ERR;
}

/*\brief
 *      Select dbname by con_redis->dbname
 *      \note con_redis must be lock  before this function.
 *\retval 0 for success
 *\retval -1 for error
 */
static int select_redis_db(struct __connectRedis *con_redis)
{
        int ret = -1;
        if(!con_redis)
                return ret;

        if(!strlen_zero(con_redis->dbname)){
                con_redis->reply = (redisReply *)redisCommand(con_redis->cxt,"select %s",con_redis->dbname);
                if(con_redis->reply && con_redis->reply->type == REDIS_REPLY_STATUS && strcasecmp(con_redis->reply->str,"OK") == 0){
                        ret = 0;
                }
                if(con_redis->reply){
                        freeReplyObject(con_redis->reply);
                        con_redis->reply=NULL;
                }
        }
        return ret;
}


static int redis_connect(struct __connectRedis * con_redis)
{
	struct timeval timeout = {1, 500000};	// 1.5 seconds

        /*here we do not invoke redis_check*/
	if (con_redis == NULL)
		return REDIS_ERR;
	if (con_redis->cxt != NULL)
                return  REDIS_OK;

	pthread_mutex_lock(&con_redis->lock);
	if (con_redis->reply != NULL){
		freeReplyObject(con_redis->reply);
                con_redis->reply = NULL;
        }

	con_redis->cxt = redisConnectWithTimeout(con_redis->ip, con_redis->port, timeout);
	if (con_redis->cxt == NULL) {
		printf( "Redis connection(%s:%d) error: can't allocate redis context.\n", con_redis->ip, con_redis->port);
                goto error;
	}
	if (con_redis->cxt->err) {
		printf( "Redis connection(%s:%d) error: %s.\n", con_redis->ip, con_redis->port, con_redis->cxt->errstr);
                goto error;
	}

	//选择指定数据库
        if(select_redis_db(con_redis))
                goto error;
        pthread_mutex_unlock(&con_redis->lock);
        return REDIS_OK;
error:
        pthread_mutex_unlock(&con_redis->lock);
        return REDIS_ERR;
}

static int redis_connection_destroy(struct __connectRedis * con_redis)
{
	if (con_redis == NULL)
		return 0;

	pthread_mutex_lock(&con_redis->lock);
	if (con_redis->reply != NULL){
		freeReplyObject(con_redis->reply);
                con_redis->reply = NULL;
        }
	if (con_redis->cxt != NULL){
		redisFree(con_redis->cxt);
                con_redis->cxt = NULL;
        }


	 pthread_mutex_unlock(&con_redis->lock);
 	 pthread_mutex_destroy(&con_redis->lock);
        if (con_redis != NULL)
                free(con_redis);

        return 0;
}

/*! \brief Check and deal with invalid circumenstance of redis connection.
 */
static int redis_detect_disconnect(struct __connectRedis * con_redis)
{
        if (con_redis == NULL)
                return REDIS_ERR;

        if (con_redis->cxt == NULL) {
                if (redis_reconnect(con_redis) == REDIS_ERR) {
                        printf("Redis Reconnect failed.\n");
                        return REDIS_ERR;
                }
        }else if(con_redis->cxt->err){
                /* if redisContext have some error lead to con_redis->reply is NULL at last time.
                 * We free old cxt, and reconnect when invoke redis-API.
                 */
                printf( "%s at last time\n", con_redis->cxt->errstr);
                printf("try reconnecting..!\n");
                redisFree(con_redis->cxt);
                con_redis->cxt = NULL;
                if (redis_reconnect(con_redis) == REDIS_ERR) {
                        printf( "Redis Reconnect failed.\n");
                        return REDIS_ERR;
                }
        }
        return REDIS_OK;
}

