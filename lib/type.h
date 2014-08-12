﻿/**
 * @file   type.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 22:24:30 2012
 * 
 * @brief  Linux WebQQ Data Struct API
 * 
 * 
 */

#ifndef LWQQ_TYPE_H
#define LWQQ_TYPE_H

#include <pthread.h>
#include <stdarg.h>
#include "lwqq-config.h"
#include "queue.h"
#include "vplist.h"

#define LWQQ_MAGIC 0x4153

#define LWQQ_DEFAULT_CATE "My Friends"
#define LWQQ_PASSERBY_CATE "Passerby"
#define LWQQ_RETRY_VALUE 3
#define LWQQ_CACHE_DIR "/tmp/lwqq"

#ifdef UNUSED
#elif defined(__GNUC__)
# 	define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
#	define UNUSED(x) x
#endif


struct LwqqHttpHandle;
struct LwqqMsgContent;
struct LwqqArguments;
struct LwqqEvents;
struct LwqqMsg;

typedef struct _LwqqHttpRequest LwqqHttpRequest;
typedef LIST_HEAD(,LwqqAsyncEntry) LwqqAsyncQueue;

typedef struct LwqqClient LwqqClient;

typedef VP_DISPATCH DISPATCH_FUNC;
typedef VP_CALLBACK CALLBACK_FUNC;
typedef vp_command  LwqqCommand;
#define _F_(f) (CALLBACK_FUNC)f
#define _C_2(d,c,...) vp_make_command(vp_func_##d,_F_(c),__VA_ARGS__)
#define _C_(d,c,...) _C_2(d,c,__VA_ARGS__)
#define _P_2(d,...) vp_make_params(vp_func_##d,__VA_ARGS__)
#define _P_(d,...) _P_2(d,__VA_ARGS__)
//return zero means continue.>1 means abort
typedef int (*LWQQ_PROGRESS)(void* data,size_t now,size_t total);
typedef char* (*LwqqHashFunc)(const char* uin,const char* ptwebqq,void* userdata);
/************************************************************************/

//=========================INSTRUCTION=================================//
/**
 * LWQQ head is defined local enum
 * WEBQQ head is defined server enum
 * that is it's value same as Server
 *
 * there are some old LWQQ head that should be WEBQQ
 */

typedef enum {
	LWQQ_WITH_LIBEV = 1<<0,
	LWQQ_WITH_LIBUV = 1<<1,
	LWQQ_WITH_SQLITE = 1<<2,
	LWQQ_WITH_MOZJS = 1<<3,
	LWQQ_WITH_SSL = 1<<4
}LwqqFeatures;

extern const LwqqFeatures lwqq_features;

typedef enum {
	LWQQ_STATUS_LOGOUT = 0,
	LWQQ_STATUS_ONLINE = 10,
	LWQQ_STATUS_OFFLINE = 20,
	LWQQ_STATUS_AWAY = 30,
	LWQQ_STATUS_HIDDEN = 40,
	LWQQ_STATUS_BUSY = 50,
	LWQQ_STATUS_CALLME = 60,
	LWQQ_STATUS_SLIENT = 70
}LwqqStatus;
typedef enum {
	LWQQ_CLIENT_PC=1,/*1-10*/
	LWQQ_CLIENT_MOBILE=21,/*21-24*/
	LWQQ_CLIENT_WEBQQ=41,
	LWQQ_CLIENT_QQFORPAD=42
}LwqqClientType;
typedef enum { 
	LWQQ_MASK_NONE = 0,
	LWQQ_MASK_1 = 1,
	LWQQ_MASK_ALL=2 
}LwqqMask;
typedef enum {
	LWQQ_MEMBER_IS_ADMIN = 0x1,
}LwqqMemberFlags;
#define LWQQ_UNKNOW 0
typedef enum {
	LWQQ_MOUTH=1,  LWQQ_CATTLE,    LWQQ_TIGER,    LWQQ_RABBIT,
	LWQQ_DRAGON,   LWQQ_SNACK,     LWQQ_HORSE,    LWQQ_SHEEP,
	LWQQ_MONKEY,   LWQQ_CHOOK,     LWQQ_DOG,      LWQQ_PIG
}LwqqShengxiao;
typedef enum {
	LWQQ_AQUARIUS=1,  LWQQ_PISCES,    LWQQ_ARIES,    LWQQ_TAURUS,
	LWQQ_GEMINI,      LWQQ_CANCER,    LWQQ_LEO,      LWQQ_VIRGO,
	LWQQ_LIBRA,       LWQQ_SCORPIO,   LWQQ_SAGITTARIUS,    LWQQ_CAPRICORNUS
}LwqqConstel;
typedef enum {
	LWQQ_BLOOD_A=1,   LWQQ_BLOOD_B,    LWQQ_BLOOD_O,
	LWQQ_BLOOD_AB,    LWQQ_BLOOD_OTHER
}LwqqBloodType;
typedef enum {
	LWQQ_FEMALE = 1,
	LWQQ_MALE = 2
}LwqqGender;
typedef enum {
	LWQQ_DEL_KEEP_OTHER = 1,/* delete buddy and keep myself from other buddy list */
	LWQQ_DEL_FROM_OTHER = 2/* delete buddy and remove myself from other buddy list */
}LwqqDelFriendType;
typedef enum {
	LWQQ_GROUP_QUN = 0,
	LWQQ_GROUP_DISCU = 1
}LwqqGroupType;
typedef enum {
	LWQQ_CLEANUP_WAITALL = 0,
	LWQQ_CLEANUP_IGNORE  = 1
}LwqqCleanUp;


/* Lwqq Error Code */
typedef enum {
	//db error
	LWQQ_EC_DB_EXEC_FAILED   = -50,
	//login error
	LWQQ_EC_FAILD_VERIFY     = -42, // verify failed by other reason
	LWQQ_EC_WRONG_VERIFY     = -41, // verify code is wrong, default
	LWQQ_EC_WRONG_PASS       = -40, 
	//response unexpected
	LWQQ_EC_NOT_JSON_FORMAT  = -30,
	//upload error code
	LWQQ_EC_UPLOAD_OVERSIZE  = -21,
	LWQQ_EC_UPLOAD_OVERRETRY = -20,
	//network error code
	LWQQ_EC_HTTP_ERROR       = -11,
	LWQQ_EC_NETWORK_ERROR    = -10,
	//system error code
	LWQQ_EC_FILE_NOT_EXIST   = -6,
	LWQQ_EC_NULL_POINTER     = -5,
	LWQQ_EC_CANCELED         = -4,
	LWQQ_EC_TIMEOUT_OVER     = -3,
	LWQQ_EC_NO_RESULT        = -2,
	LWQQ_EC_ERROR            = -1,

	//webqq error code
	LWQQ_EC_OK               = 0,
	LWQQ_EC_LOGIN_NEED_VC    = 10,
	LWQQ_EC_HASH_WRONG       = 50,
	LWQQ_EC_LOGIN_ABNORMAL   = 60,///<登录需要解禁
	LWQQ_EC_NO_MESSAGE       = 102,
	LWQQ_EC_COOKIE_WRONG     = 103,
	LWQQ_EC_PTWEBQQ          = 116,
	LWQQ_EC_LOST_CONN        = 121,
	LWQQ_EC_LOGIN_NEED_BARCODE   = 10005
} LwqqErrorCode;

#define LWQQ_FRIEND_CATE_IDX_DEFAULT 0
#define LWQQ_FRIEND_CATE_IDX_PASSERBY -1

typedef struct LwqqHashEntry {
	const char* name;
	LwqqHashFunc func;
	void* data;
}LwqqHashEntry;

/* Struct defination */
typedef struct LwqqFriendCategory {
	int index;
	int sort;
	char *name;
	int count;
	LIST_ENTRY(LwqqFriendCategory) entries;
} LwqqFriendCategory;

//means the buddy need update by hand
#define LWQQ_LAST_MODIFY_RESET 0
//means the buddy is not in database
#define LWQQ_LAST_MODIFY_UNKNOW -1
/* QQ buddy */
typedef struct LwqqBuddy {
	char *uin;                  /**< Uin. Change every login */
	char *qqnumber;             /**< QQ number */
	char *face;
	char *occupation;
	char *phone;
	char *allow;
	char *college;
	char *reg_time;
	LwqqConstel constel;
	LwqqBloodType blood;
	char *homepage;
	char *country;
	char *city;
	char *personal;
	char *nick;
	char *long_nick;
	LwqqShengxiao shengxiao;
	char *email;
	char *province;
	LwqqGender gender;
	char *mobile;
	char *vip_info;
	char *markname;
	LwqqStatus stat;
	LwqqClientType client_type;
	time_t birthday;
	char *flag;
	int cate_index;           /**< Index of the category */
	//extra data
	char *avatar;
	size_t avatar_len;
	time_t last_modify;
	char *token;                /**< Only used in add friend */
	void *data;                 /**< user defined data */
	int level;
	LIST_ENTRY(LwqqBuddy) entries; /* FIXME: Do we really need this? */
} LwqqBuddy;


typedef LIST_HEAD(LwqqFriendList,LwqqBuddy) 
	LwqqFriendList;
	typedef struct LwqqSimpleBuddy{
		char* uin;
		char* qq;
		char* nick;
		char* card;                 /* 群名片 */
		LwqqClientType client_type;
		LwqqStatus stat;
		LwqqMemberFlags mflag;
		char* cate_index;
		char* group_sig;            /* only use at sess message */
		LIST_ENTRY(LwqqSimpleBuddy) entries;
	}LwqqSimpleBuddy;

/* QQ group */
typedef struct LwqqGroup {
	LwqqGroupType type;
	char *name;                  /**< QQ Group name */
	union{
		char *gid;                   /**< QQ Group id */
		char *did;                   /**< QQ Discu id */
	};
	union{
		char *account;               /** < Group: QQ number 
Discu: (with lwdb) the only one and stable key reference
(without lwdb) equal to info_seq;
*/
		char *qq;                    /** < QQ number */
	};
	int  info_seq;               /** < avaliable for discu */
	char *code;    
	char *markname;              /** < QQ Group mark name */

	/* ginfo */
	char *face;
	char *memo;
	char *classes;
	char *fingermemo;
	time_t createtime;
	char *level;
	char *owner;                 /** < owner's QQ number  */
	char *flag;
	char *option;
	LwqqMask mask;             /** < group mask */

	char *group_sig;            /** < use in sess msg */

	int last_seq;               // readonly: received last message seq
	time_t last_modify;
	char *avatar;
	size_t avatar_len;
	void *data;                 /** < user defined data */

	LIST_ENTRY(LwqqGroup) entries;
	LIST_HEAD(, LwqqSimpleBuddy) members; /** < QQ Group members */
	LwqqAsyncQueue ev_queue;
} LwqqGroup;
#define lwqq_member_is_founder(member,group) (strcmp(member->uin,group->owner)==0)
#define lwqq_group_is_qun(group) (group->type==LWQQ_GROUP_QUN)
#define lwqq_group_is_discu(group) (group->type==LWQQ_GROUP_DISCU)

typedef struct LwqqVerifyCode {
	char *str; // input
	char *uin;
	//{image
	char *data;
	size_t size;
	//}
	LwqqClient* lc;
	LwqqCommand cmd;
} LwqqVerifyCode ;

typedef enum {LWQQ_NO,LWQQ_YES,LWQQ_EXTRA_ANSWER,LWQQ_IGNORE} LwqqAnswer;
#define LWQQ_ALLOW_AND_ADD LWQQ_EXTRA_ANSWER


/* LwqqClient API */
struct LwqqClient {
	char *username;             /**< Username */
	char *password;             /**< Password */
	char *version;              /**< WebQQ version */
	char *clientid;
	char *seskey;
	char *cip;
	char *index;
	char *port;
	char *vfwebqq;
	char *psessionid;
	const char* last_err;
	char *gface_key;                  /**< use at cface */
	char *gface_sig;                  /**<use at cfage */
	char *login_sig;
	char *error_description;
	char *new_ptwebqq;              /**< this only used when relogin */

	LwqqBuddy *myself;          /**< Myself */
	LwqqVerifyCode *vc;         /**< Verify Code */
	struct LwqqEvents * events;
	struct LwqqArguments * args;
	struct LwqqRecvMsgList *msg_list;

	LwqqStatus stat;

	void (*dispatch)(LwqqCommand,unsigned long timeout);
	LwqqBuddy* (*find_buddy_by_uin)(struct LwqqClient* lc,const char* uin);
	LwqqBuddy* (*find_buddy_by_qqnumber)(struct LwqqClient* lc,const char* qqnumber);


	LwqqFriendList friends; /**< QQ friends */
	LIST_HEAD(, LwqqFriendCategory) categories; /**< QQ friends categories */
	LIST_HEAD(, LwqqGroup) groups; /**< QQ groups */
	LIST_HEAD(, LwqqGroup) discus; /**< QQ discus */

	LwqqAsyncQueue ev_queue;
	/** non data area **/
	void* data;                     /**< user defined data*/
	int magic;          /**< 0x4153 **/
};
#define lwqq_client_userdata(lc) (lc->data)
#define lwqq_client_dispatch(lc,cmd) (lc->dispatch(cmd,0))

/**
 * this is used for some long http request actions chain. 
 * because event tigger only once. while async option often calls repeatly.
 * and most of async option require gui display some information.
 *
 */
typedef struct LwqqEvents
{
	/**
	 * for test only
	 * modify: login_ec <- 2
	 */
	LwqqCommand start_login;
	/**
	 * this is login complete .whatever successed or failed
	 * except need verify code
	 */
	LwqqCommand login_complete;
	/* 
	 * this is very important when poll message come 
	 */
	LwqqCommand poll_msg;
	/* this is poll lost after recv retcode 112 or 108 */
	LwqqCommand poll_lost;
	/* this is upload content failed such as lwqq offline pic */
	LwqqCommand upload_fail;
	/* this is you confirmed a friend request 
	 * you should add buddy to gui level.
	 */
	LwqqCommand new_friend;
	LwqqCommand new_group;
	LwqqCommand need_verify;
	/* this called when successfully delete group from server
	 * and the last chance to visit group
	 * do not delete group in this function
	 * it would deleted later.
	 */
	LwqqCommand delete_group;
	/** this called when group member changes
	 * you need flush displayed group member
	 */
	LwqqCommand group_member_chg;
	LwqqCommand ext_clean;
	/** a group information has changed
	 *  modify : buddy <- changed buddy
	 */
	LwqqCommand friend_chg;
	/** a group information has changed
	 *  modify : group <- changed group
	 */
	LwqqCommand group_chg;
}LwqqEvents;

LwqqEvents* lwqq_client_get_events(LwqqClient* lc);

typedef struct LwqqArguments
{
	LwqqErrorCode login_ec;
	LwqqBuddy* buddy;
	LwqqGroup* group;
	LwqqVerifyCode* vf_image;
	const LwqqGroup* deleted_group;
	const char* serv_id;
	struct LwqqMsgContent* content;
	LwqqErrorCode err;
	char* hash_result;
}LwqqArguments;

LwqqArguments* lwqq_client_get_args(LwqqClient* lc);

/** add repeat event listener to lc->events->...
 * @param : event <- a pointer to lc->events->...
 *          cmd   <- _C_ macro
 * @return <- an id used to unlink
 */
const LwqqCommand* lwqq_add_event_listener(LwqqCommand* event,LwqqCommand cmd);
#define lwqq_add_event(event,cmd) lwqq_add_event_listener(&event,cmd);

/* Struct defination end */

/************************************************************************/
/* LwqqClient API */
/** 
 * Create a new lwqq client
 * 
 * @param username QQ username
 * @param password QQ password
 * 
 * @return A new LwqqClient instance, or NULL failed
 */
LwqqClient *lwqq_client_new(const char *username, const char *password);

#define lwqq_client_valid(lc) (lc!=0&&lc->magic==LWQQ_MAGIC)
#define lwqq_client_logined(lc) (lc->stat != LWQQ_STATUS_LOGOUT)

/** 
 * Free LwqqVerifyCode object
 * 
 * @param vc 
 */
void lwqq_vc_free(LwqqVerifyCode *vc);

/** 
 * Free LwqqClient instance
 * 
 * @param client LwqqClient instance
 */
void lwqq_client_free(LwqqClient *client);
struct LwqqHttpHandle* lwqq_get_http_handle(LwqqClient* lc);

/* LwqqClient API end */

/************************************************************************/
/* LwqqBuddy API  */
/** 
 * 
 * Create a new buddy
 * 
 * @return A LwqqBuddy instance
 */
LwqqBuddy *lwqq_buddy_new();
LwqqSimpleBuddy* lwqq_simple_buddy_new();

//add the ref count of buddy.
//that means only ref count down to zero.
//we really free the memo space.
#define lwqq_buddy_ref(buddy) buddy->ref++;
/** 
 * Free a LwqqBuddy instance
 * 
 * @param buddy 
 */
void lwqq_buddy_free(LwqqBuddy *buddy);
void lwqq_simple_buddy_free(LwqqSimpleBuddy* buddy);

/** 
 * Find buddy object by buddy's uin member
 * 
 * @param lc Our Lwqq client object
 * @param uin The uin of buddy which we want to find
 * 
 * @return 
 */
LwqqBuddy *lwqq_buddy_find_buddy_by_uin(LwqqClient *lc, const char *uin);
LwqqBuddy *lwqq_buddy_find_buddy_by_qqnumber(LwqqClient *lc, const char *qqnumber);
/**
 * Find buddy object by buddy's nick or mark name
 */
LwqqBuddy* lwqq_buddy_find_buddy_by_name(LwqqClient* lc,const char* name);


LwqqFriendCategory* lwqq_category_find_by_name(LwqqClient* lc,const char* name);
LwqqFriendCategory* lwqq_category_find_by_id(LwqqClient* lc,int index);

/* LwqqBuddy API END*/


/** 
 * Free a LwqqGroup instance
 * 
 * @param group
 */
void lwqq_group_free(LwqqGroup *group);

/** 
 * 
 * Create a new group
 * @param type LWQQ_GROUP_DISCU or LWQQ_GROUP_QUN
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_new(int type);

/** 
 * Find group object or discus object by group's gid member
 * 
 * @param lc Our Lwqq client object
 * @param uin The gid of group which we want to find
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_find_group_by_gid(LwqqClient *lc, const char *gid);
#define lwqq_discu_find_discu_by_did(lc,did) lwqq_group_find_group_by_gid(lc,did);
LwqqGroup* lwqq_group_find_group_by_qqnumber(LwqqClient* lc,const char* qqnumber);
#define lwqq_group_find_group_by_account(lc,acc) lwqq_group_find_group_by_qqnumber(lc,acc)

/** 
 * Find group member object by member's uin
 * 
 * @param group Our Lwqq group object
 * @param uin The uin of group member which we want to find
 * 
 * @return A LwqqBuddy instance 
 */
LwqqSimpleBuddy *lwqq_group_find_group_member_by_uin(LwqqGroup *group, const char *uin);

#define format_append(str,format...)\
	snprintf(str+strlen(str),sizeof(str)-strlen(str),##format)

/** auto select hash function, it try one form system queue, if failed, try
 * next one, you can set begin postion to start scan */
char* lwqq_hash_auto(const char* uin, const char* ptwebqq, void* lc);
/* check we have already tried all hash */
int lwqq_hash_all_finished(LwqqClient* lc);
/* register a new js entry for auto select.
 * note we have only 8 size, and lwqq used 4, so you can only add 3 at most,
 * last one must be empty*/
void lwqq_hash_add_entry(LwqqClient* lc, const char* name, LwqqHashFunc func, void* data);
/* set the begin postion to start auto select */
void lwqq_hash_set_beg(LwqqClient* lc, const char* hash_name);
/* get the last one successful hash, you should save it for next time use */
const LwqqHashEntry* lwqq_hash_get_last(LwqqClient* lc);

/************************************************************************/

const char* lwqq_status_to_str(LwqqStatus status);
LwqqStatus lwqq_status_from_str(const char* str);
const char* lwqq_date_to_str(time_t date);

//long time
#define LTIME lwqq_time()
long lwqq_time();



#endif  /* LWQQ_TYPE_H */

// vim: ts=3 sw=3 sts=3 noet
