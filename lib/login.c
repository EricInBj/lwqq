/**
 * @file   login.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 02:21:43 2012
 * 
 * @brief  Linux WebQQ Login API
 *  Login logic is based on the gtkqq implementation
 *  written by HuangCongyu <huangcongyu2006@gmail.com>
 * 
 * 
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "login.h"
#include "logger.h"
#include "http.h"
#include "smemory.h"
#include "md5.h"
#include "url.h"
#include "json.h"
#include "async.h"
#include "utility.h"
#include "internal.h"
#include "lwjs.h"

/* URL for webqq login */
#define APPID "1003903"


static LwqqAsyncEvent* set_online_status(LwqqClient *lc,const char *status);
static int get_version_back(LwqqHttpRequest* req);
static int do_login_back(LwqqHttpRequest* req,LwqqAsyncEvent*);
static int set_online_status_back(LwqqHttpRequest* req);
static void login_stage_2(LwqqClient* lc,LwqqErrorCode* err);
static void login_stage_3(LwqqAsyncEvent* ev,LwqqErrorCode* ec);
static void login_stage_4(LwqqClient* lc,LwqqErrorCode* ec);
static void login_stage_5(LwqqAsyncEvent* ev,LwqqErrorCode* ec);
static void login_stage_6(LwqqAsyncEvent* ev, LwqqErrorCode* ec);

static int get_login_sig_back(LwqqHttpRequest* req)
{
	LwqqErrorCode err = LWQQ_EC_OK;
	LwqqClient* lc = req->lc;
	lwqq__jump_if_http_fail(req, err);
	if(!req->response){err = LWQQ_EC_ERROR;goto done;}
	char* beg = strstr(req->response,"var g_login_sig=");
	char login_sig[64];
	sscanf(beg,"var g_login_sig=encodeURIComponent(\"%[^\"]\")",login_sig);
	lwqq_override(lc->login_sig, s_strdup(login_sig));
done:
	lwqq_http_request_free(req);
	return err;
}

static LwqqAsyncEvent* get_login_sig(LwqqClient* lc)
{
	char url[512];
	snprintf(url,sizeof(url),WEBQQ_LOGIN_UI_HOST"/cgi-bin/login"
			"?daid=164&target=self&style=5&mibao_css=m_webqq&appid=1003903"
			"&enable_qlogin=0&s_url=http%%3A%%2F%%2Fweb2.qq.com%%2Floginproxy.html"
			);
	LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
	lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT,5);
	lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT_INCRE,5);
	return req->do_request_async(req,lwqq__hasnot_post(),_C_(p_i,get_login_sig_back,req));
}
static int check_need_verify_back(LwqqHttpRequest* req)
{
	int err = LWQQ_EC_OK;
	LwqqClient* lc = req->lc;
	if(req->err != LWQQ_EC_OK){
		err = LWQQ_EC_NETWORK_ERROR;
		lc->args->login_ec = err;
		vp_do_repeat(lc->events->login_complete, NULL);
		goto done;
	}
	lwqq__jump_if_http_fail(req, err);

	/**
	 * 
	 * The http message body has two format:
	 *
	 * ptui_checkVC('1','9ed32e3f644d968809e8cbeaaf2cce42de62dfee12c14b74','\x00\x00\x00\x00\x04\x7e\x73\xb2');
	 * ptui_checkVC('0','!IJG', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
	 * The former means we need verify code image and the second
	 * parameter is vc_type.
	 * The later means we don't need the verify code image. The second
	 * parameter is the verify code. The vc_type is in the header
	 * "Set-Cookie".
	 */
	int need_vf;
	char param2[256];
	char param3[256];
	sscanf(req->response,"ptui_checkVC('%d','%[^']','%[^']');",&need_vf,param2,param3);
	lc->vc = s_malloc0(sizeof(*lc->vc));
	lc->vc->uin = s_strdup(param3);
	lc->vc->str = s_strdup(param2);
	lc->vc->lc  = lc;

	if (need_vf == 0) {
		/* We need get the ptvfsession from the header "Set-Cookie" */
		char* pt_verifysession = lwqq_http_get_cookie(req, "ptvfsession");
		lc->pt_verifysession = pt_verifysession;
//		s_free(pt_verifysession);
		lwqq_log(LOG_NOTICE, "Verify code: %s\n", lc->vc->str);
	} else if (need_vf == 1) {
//		lc->pt_verifysession = lwqq_http_get_cookie(req,"verifysession");
		err = LWQQ_EC_LOGIN_NEED_VC;
		lwqq_log(LOG_NOTICE, "We need verify code image: %s\n", lc->vc->str);
	}

done:
	lwqq_http_request_free(req);
	return err;
}

static LwqqAsyncEvent* check_need_verify(LwqqClient *lc,const char* appid)
{
	LwqqHttpRequest *req;
	char url[512];
	char buf[256];

	srand48(time(NULL));
	double random = drand48();
	snprintf(url, sizeof(url), WEBQQ_CHECK_HOST"/check?uin=%s&appid=%s&"
			"js_ver=10113&js_type=0&%s%s&pt_tea=1&u1=http%%3A%%2F%%2Fweb.qq.com%%2Floginproxy.html&r=%.16lf",
			lc->username, appid,
			lc->login_sig?"login_sig=":"",
			lc->login_sig?:"",
			random);
	req = lwqq_http_create_default_request(lc,url,NULL);
	req->set_header(req,"Referer",WEBQQ_LOGIN_LONG_REF_URL(buf));

	return req->do_request_async(req, lwqq__hasnot_post(),_C_(p_i,check_need_verify_back,req));
}


static int request_captcha_back(LwqqHttpRequest* req,LwqqVerifyCode* code)
{
	int err = 0;
	if(req->http_code!=200){
		err = -1;
		goto done;
	}
	LwqqClient* lc = req->lc;
	code->data = req->response;
	code->size = req->resp_len;
	req->response = NULL;
	lc->args->vf_image = code;
	char* pt_verifysession = lwqq_http_get_cookie(req, "verifysession");
	lc->pt_verifysession = pt_verifysession;

	vp_do_repeat(lc->events->need_verify, NULL);
done:
	lwqq_http_request_free(req);
	return err;
}

static LwqqAsyncEvent* get_verify_image(LwqqClient *lc)
{
	LwqqHttpRequest *req = NULL;  
	char url[512];
	char chkuin[64];
	LwqqErrorCode err;

	snprintf(url, sizeof(url), WEBQQ_CAPTCHA_HOST"/getimage?aid=%s&uin=%s", APPID, lc->username);
	req = lwqq_http_create_default_request(lc,url, &err);

	snprintf(chkuin, sizeof(chkuin), "chkuin=%s", lc->username);
	req->set_header(req, "Cookie", chkuin);
	return req->do_request_async(req, lwqq__hasnot_post(),_C_(2p_i,request_captcha_back,req,lc->vc));
}

static void upcase_string(char *str, int len)
{
	int i;
	for (i = 0; i < len; ++i) {
		if (islower(str[i]))
			str[i]= toupper(str[i]);
	}
}



char * replace(
		char const * const original, 
		char const * const pattern, 
		char const * const replacement
		) {
	size_t const replen = strlen(replacement);
	size_t const patlen = strlen(pattern);
	size_t const orilen = strlen(original);

	size_t patcnt = 0;
	const char * oriptr;
	const char * patloc;

	// find how many times the pattern occurs in the original string
	for (oriptr = original; patloc = strstr(oriptr, pattern); oriptr = patloc + patlen)
	{
		patcnt++;
	}

	{
		// allocate memory for the new string
		size_t const retlen = orilen + patcnt * (replen - patlen);
		char * const returned = (char *) malloc( sizeof(char) * (retlen + 1) );

		if (returned != NULL)
		{
			// copy the original string, 
			// replacing all the instances of the pattern
			char * retptr = returned;
			for (oriptr = original; patloc = strstr(oriptr, pattern); oriptr = patloc + patlen)
			{
				size_t const skplen = patloc - oriptr;
				// copy the section until the occurence of the pattern
				strncpy(retptr, oriptr, skplen);
				retptr += skplen;
				// copy the replacement 
				strncpy(retptr, replacement, replen);
				retptr += replen;
			}
			//                                                                                                                                   // copy the rest of the string.
			strcpy(retptr, oriptr);
		}
		return returned;
	}
}




/**
 * I hacked the javascript file named comm.js, which received from tencent
 * server, and find that fuck tencent has changed encryption algorithm
 * for password in webqq3 . The new algorithm is below(descripted with javascript):
 * var M=C.p.value; // M is the qq password 
 * var I=hexchar2bin(md5(M)); // Make a md5 digest
 * var H=md5(I+pt.uin); // Make md5 with I and uin(see below)
 * var G=md5(H+C.verifycode.value.toUpperCase());
 * 
 * @param pwd User's password
 * @param vc Verify Code. e.g. "!M6C"
 * @param uin A string like "\x00\x00\x00\x00\x54\xb3\x3c\x53", NB: it
 *        must contain 8 hexadecimal number, in this example, it equaled
 *        to "0x0,0x0,0x0,0x0,0x54,0xb3,0x3c,0x53"
 * 
 * @return Encoded password on success, else NULL on failed
 */
static char *lwqq_enc_pwd(const char *pwd, const char *vc, const char *uin)
{
	return lwqq_js_enc_pwd(pwd,replace(uin,"\\","-"),vc);
	int i;
	int uin_byte_length;
	char buf[128] = {0};
	unsigned char sig[32];
	char _uin[9] = {0};

	if (!pwd || !vc || !uin) {
		lwqq_log(LOG_ERROR, "Null parameterment\n");
		return NULL;
	}


	/* Calculate the length of uin (it must be 8?) */
	uin_byte_length = strlen(uin) / 4;

	/**
	 * Ok, parse uin from string format.
	 * "\x00\x00\x00\x00\x54\xb3\x3c\x53" -> {0,0,0,0,54,b3,3c,53}
	 */
	for (i = 0; i < uin_byte_length ; i++) {
		char u[5] = {0};
		char tmp;
		strncpy(u, uin + i * 4 + 2, 2);

		errno = 0;
		tmp = strtol(u, NULL, 16);
		if (errno) {
			return NULL;
		}
		_uin[i] = tmp;
	}
	char* salt = s_strdup(_uin);
	/* Equal to "var I=hexchar2bin(md5(M));" */
	md5_buffer(pwd,strlen(pwd),sig);
	memcpy(buf,sig,sizeof(sig));

	/* Equal to "var H=md5(I+pt.uin);" */
	memcpy(buf + 16, _uin, uin_byte_length);
	md5_buffer(buf, 16 + uin_byte_length, sig);
	md5_sig_to_string(sig,buf,sizeof(buf));

	/* Equal to var G=md5(H+C.verifycode.value.toUpperCase()); */
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", vc);
	upcase_string(buf, strlen(buf));

	md5_buffer(buf, strlen(buf), sig);
	md5_sig_to_string(sig,buf,sizeof(buf));
	upcase_string(buf, strlen(buf));

	/* OK, seems like every is OK */
	return s_strdup(buf);
}



/** 
 * Do really login
 * 
 * @param lc 
 * @param md5 The md5 calculated from calculate_password_md5() 
 * @param err 
 */
static LwqqAsyncEvent* do_login(LwqqClient *lc, const char *md5, LwqqErrorCode *err)
{
	char url[1024];
	char refer[1024];
	LwqqHttpRequest *req;

	snprintf(url, sizeof(url), WEBQQ_LOGIN_HOST"/login?"
			"u=%s"
			"&p=%s"
			"&verifycode=%s"
			"&webqq_type=%d"
			"&remember_uin=1"
			"&aid=1003903"
			"&login2qq=1"
			"&u1=http%%3A%%2F%%2Fweb2.qq.com%%2Floginproxy.html%%3Flogin2qq%%3D1%%26webqq_type%%3D10"
			"&h=1"
			"&ptredirect=0"
			"&ptlang=2052"
			"&daid=164"
			"&from_ui=1"
			"&pttype=1"
			"&dumy="
			"&fp=loginerroralert"
			"&action=2-10-5837"
			"&mibao_css=m_webqq"
			"&t=1"
			"&g=1"
			"&js_type=0"
			"&js_ver=10113"
			"&login_sig=%s"
			"&pt_uistyle=5"
			"&pt_randsalt=0"
			"&pt_vcode_v1=0"
			"&pt_verifysession_v1=%s",
			lc->username, md5, lc->vc->str,lc->stat,lc->login_sig,lc->pt_verifysession?:"");

	req = lwqq_http_create_default_request(lc,url, err);
	/* Setup http header */
	req->set_header(req, "Referer", WEBQQ_LOGIN_LONG_REF_URL(refer));

	LwqqAsyncEvent* ret = lwqq_async_event_new(NULL);
	/* Send request */
	LwqqAsyncEvent* ev = req->do_request_async(req, lwqq__hasnot_post(),_C_(2p_i,do_login_back,req,ret));
	lwqq_async_add_event_chain(ev,ret);
	return ret;
}

static int do_login_back(LwqqHttpRequest* req,LwqqAsyncEvent* event)
{
	LwqqClient* lc = req->lc;
	int err = LWQQ_EC_OK;
	const char* response;
	//const char redirect_url[512];
	if (req->http_code != 200) {
		err = LWQQ_EC_HTTP_ERROR;
		goto done;
	}
	if (strstr(req->response,"aq.qq.com")!=NULL){
		err = LWQQ_EC_LOGIN_ABNORMAL;
		const char* beg = strstr(req->response,"http://aq.qq.com");
		if(beg) sscanf(beg,"%[^']",lc->error_description); // beg may be null
		goto done;
	}
	if(req->response == NULL){
		lwqq_puts("login no response\n");
		err = LWQQ_EC_NETWORK_ERROR;
		goto done;
	}

	response = req->response;
	lwqq_verbose(3,"%s\n",response);
	char *p = strstr(response, "\'");
	if (!p) {
		err = LWQQ_EC_ERROR;
		goto done;
	}
	int status,param2;
	char url[512];
	int param4;
	char msg[512];
	char user[64];
	//void url is '' which makes sscanf failed
	//%*c is for eat a blank before 'user'
	sscanf(response,"ptuiCB('%d','%d','%[^,],'%d','%[^']',%*c'%[^']');",
			&status,&param2,url,&param4,msg,user);
	url[strlen(url)-1]=0;
	switch (status) {
		case 0:
			{
				err = LWQQ_EC_OK;
				// goint to check_sig
				LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
				LwqqAsyncEvent* ev = req->do_request_async(req,lwqq__hasnot_post(),_C_(p_i,lwqq__process_empty,req));
				lwqq_async_add_event_chain(ev, event);
			} break;
		case 1:
			lwqq_log(LOG_WARNING, "Server busy! Please try again\n");
			lc->last_err = "Server busy! Please try again";
			err = LWQQ_EC_ERROR;
			goto done;

		case 2:
			lwqq_log(LOG_ERROR, "Out of date QQ number\n");
			lc->last_err = "Out of date QQ number";
			err = LWQQ_EC_ERROR;
			goto done;

		case 3:
			lwqq_log(LOG_ERROR, "Wrong password\n");
			err = LWQQ_EC_WRONG_PASS;
			lc->last_err = "Wrong username or password";
			goto done;

		case 4:
			lwqq_log(LOG_ERROR, "Wrong verify code\n");
			err = LWQQ_EC_WRONG_VERIFY;
			lc->last_err = "Wrong verify code";
			goto done;

		case 5:
			lwqq_log(LOG_ERROR, "Verify failed\n");
			lc->last_err = "Verify failed";
			err = LWQQ_EC_FAILD_VERIFY;
			goto done;

		case 6:
		case 71: //ptuiCB('71','0','','0','您的帐号由于异常禁止登录，请联系客服。(2009081021)', 'QQNUMBER');
			lwqq_log(LOG_WARNING, "You may need to try login again\n");
			lc->last_err = "You may need to try login again";
			err = LWQQ_EC_ERROR;
			goto done;

		case 7:
			lwqq_log(LOG_ERROR, "Wrong input\n");
			lc->last_err = "Wrong input";
			err = LWQQ_EC_ERROR;
			goto done;

		case 8:
			lwqq_log(LOG_ERROR, "Too many logins on this IP. Please try again\n");
			lc->last_err = "Too many logins on this IP.Please try again";
			err = LWQQ_EC_ERROR;
			goto done;
		case LWQQ_EC_LOGIN_NEED_BARCODE:
			lwqq_log(LOG_ERROR, "%s\n",msg);
			lc->error_description = s_strdup(msg);
			err = LWQQ_EC_LOGIN_NEED_BARCODE;
			goto done;

		default:
			err = LWQQ_EC_ERROR;
			lc->last_err = "Unknow error";
			lwqq_log(LOG_ERROR, "Unknow error");
			goto done;
	}

done:
	lwqq_http_request_free(req);
	return err;
}

/**
 * Get WebQQ version from tencent server
 * The response is like "ptuiV(201205211530)", what we do is get "201205211530"
 * from the response and set it to lc->version.
 * 
 * @param lc 
 * @param err *err will be set LWQQ_EC_OK if everything is ok, else
 *        *err will be set LWQQ_EC_ERROR.
 */

LWQQ_EXPORT
LwqqAsyncEvent* lwqq_get_version(LwqqClient *lc, LwqqErrorCode *err)
{
	LwqqHttpRequest *req;

	const char* url = WEBQQ_VERSION_URL;
	req = lwqq_http_create_default_request(lc,url , err);
	lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT,5L);

	/* Send request */
	lwqq_log(LOG_DEBUG, "Get webqq version from %s\n", WEBQQ_VERSION_URL);
	return  req->do_request_async(req, lwqq__hasnot_post(),_C_(p_i,get_version_back,req));
}
static int get_version_back(LwqqHttpRequest* req)
{
	int err = LWQQ_EC_OK;
	char* response = NULL;
	LwqqClient* lc = req->lc;
	if(!lwqq_client_valid(lc)){
		err = LWQQ_EC_ERROR;
		goto done;
	}
	if (req->http_code!=200) {
		err = LWQQ_EC_NETWORK_ERROR;
		goto done;
	}
	response = req->response;
	if(response == NULL){
		err = LWQQ_EC_NETWORK_ERROR;
		goto done;
	}
	if (strstr(response, "ptuiV")) {
		char *s, *t;
		char *v;
		s = strchr(response, '(');
		t = strchr(response, ')');
		if (!s || !t) {
			err = LWQQ_EC_ERROR;
			goto done;
		}
		s++;
		v = malloc(t - s + 1);
		memset(v, 0, t - s + 1);
		strncpy(v, s, t - s);
		s_free(lc->version);
		lc->version = s_strdup(v);
		err = LWQQ_EC_OK;
		s_free(v);
	}

done:
	lwqq_http_request_free(req);
	return err;
}

static char *generate_clientid()
{
	int r;
	struct timeval tv;
	long t;
	char buf[20] = {0};

	srand(time(NULL));
	r = rand() % 90 + 10;
	if (gettimeofday(&tv, NULL)) {
		return NULL;
	}
	t = tv.tv_usec % 1000000;
	snprintf(buf, sizeof(buf), "%2d%6ld", r, t);
	return s_strdup(buf);
}

/** 
 * Set online status, this is the last step of login
 * 
 * @param err
 * @param lc 
 */
static LwqqAsyncEvent* set_online_status(LwqqClient *lc,const char *status)
{
	char msg[1024] ={0};
	char* post = msg;
	LwqqHttpRequest *req = NULL;  

	if (!status) {
		return NULL;
	}

	lc->clientid = generate_clientid();
	if (!lc->clientid) {
		lwqq_log(LOG_ERROR, "Generate clientid error\n");
		return NULL;
	}

	/* Create a POST request */
	char url[512] ={0};
	snprintf(url,sizeof(url),"%s/channel/login2",WEBQQ_D_HOST);
	req = lwqq_http_create_default_request(lc,url, NULL);
	LWQQ_DEBUG(lwqq_http_set_option(req, LWQQ_HTTP_VERBOSE, 1L));

	lwqq_override(lc->session.ptwebqq, lwqq_http_get_cookie(req, "ptwebqq"));
	snprintf(msg, sizeof(msg), "r={\"status\":\"%s\",\"ptwebqq\":\"%s\","
			"\"passwd_sig\":""\"\",\"clientid\":\"%s\""
			", \"psessionid\":null}&clientid=%s&psessionid=null"
			,status, lc->session.ptwebqq
			,lc->clientid, lc->clientid);
	urlencode(msg, 2);

	/* Set header needed by server */
	req->set_header(req, "Referer", WEBQQ_D_REF_URL);
	req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
	return  req->do_request_async(req, lwqq__has_post(),_C_(p_i,set_online_status_back,req));
}

static int set_online_status_back(LwqqHttpRequest* req)
{
	int err = 0;
	LwqqClient* lc = req->lc;
	json_t* root = NULL,*result;
	lwqq__jump_if_http_fail(req,err);
	lwqq__jump_if_json_fail(root,req->response,err);
	result = lwqq__parse_retcode_result(root, &err);
	if(err) goto done;
	if(result){
		lwqq_override(lc->myself->uin,lwqq__json_get_value(result,"uin"));
		lwqq_override(lc->seskey,lwqq__json_get_value(result,"seskey"));
		lwqq_override(lc->cip,lwqq__json_get_value(result,"cip"));
		lwqq_override(lc->index,lwqq__json_get_value(result,"index"));
		lwqq_override(lc->port,lwqq__json_get_value(result,"port"));
		lwqq_override(lc->psessionid,lwqq__json_get_value(result,"psessionid"));
		lwqq_override(lc->vfwebqq,lwqq__json_get_value(result,"vfwebqq"));
		lc->stat = lwqq_status_from_str(json_parse_simple_value(result, "status"));
	}
done:
	lwqq__log_if_error(err, req);
	lwqq__clean_json_and_req(root,req);
	return err;
}

/** 
 * WebQQ login function
 * Step:
 * 1. Get webqq version
 * 2. Get verify code
 * 3. Calculate password's md5
 * 4. Do real login 
 * 5. check whether logining successfully
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
LWQQ_EXPORT 
void lwqq_login(LwqqClient *client, LwqqStatus status,LwqqErrorCode *err)
{
	if (!client) {
		lwqq_log(LOG_ERROR, "Invalid pointer\n");
		return ;
	}
	if(status == LWQQ_STATUS_LOGOUT || status == LWQQ_STATUS_OFFLINE){
		lwqq_log(LOG_WARNING, "Invalid online status\n");
		return;
	}

	client->stat = status;

	client->args->login_ec = LWQQ_EC_NO_RESULT;
	vp_do_repeat(client->events->start_login, NULL);
	/* optional: get webqq version */
	//get_version(client, err);
	if(!client->vc){
		LwqqAsyncEvent* ev = get_login_sig(client);
		lwqq_async_add_event_listener(ev, _C_(2p,login_stage_2,client,err));
	}else{
		login_stage_4(client,err);
	}
}

static void login_stage_2(LwqqClient* lc,LwqqErrorCode* err)
{
	if(!lwqq_client_valid(lc)) err&&(*err=LWQQ_EC_ERROR);

	/**
	 * Second, we get the verify code from server.
	 * If server provide us a image and let us enter code shown
	 * in image number, in this situation, we just return LWQQ_EC_LOGIN_NEED_VC
	 * simply, so user should call lwqq_login() again after he set correct
	 * code to vc->str;
	 * Else, if we can get the code directly, do login immediately.
	 * 
	 */
	if (!lc->vc) {
		LwqqAsyncEvent* ev = check_need_verify(lc,APPID);
		lwqq_async_add_event_listener(ev,_C_(2p,login_stage_3,ev,err));
		return;
	}

	login_stage_4(lc,err);
}

static void login_stage_3(LwqqAsyncEvent* ev,LwqqErrorCode* ec)
{
	int err = ev->result;
	if(ec) *ec=err;
	LwqqClient* lc = ev->lc;
	if(!lwqq_client_valid(lc)) return;
	switch (err) {
		case LWQQ_EC_LOGIN_NEED_VC:
			lwqq_log(LOG_WARNING, "Need to enter verify code\n");
			lc->vc->cmd = _C_(2p,login_stage_4,lc,ec);
			get_verify_image(lc);
			return ;

		case LWQQ_EC_NETWORK_ERROR:
			lwqq_log(LOG_ERROR, "Network error\n");
			lc->stat = LWQQ_STATUS_LOGOUT;
			lc->args->login_ec = err;
			vp_do_repeat(lc->events->login_complete, NULL);
			return ;

		case LWQQ_EC_OK:
			lwqq_log(LOG_DEBUG, "Get verify code OK\n");
			break;

		default:
			lwqq_log(LOG_ERROR, "Unknown error\n");
			lc->stat = LWQQ_STATUS_LOGOUT;
			lc->args->login_ec = err;
			vp_do_repeat(lc->events->login_complete, NULL);
			return ;
	}

	login_stage_4(lc,ec);
}

static void login_stage_4(LwqqClient* lc,LwqqErrorCode* ec)
{
	if(!lwqq_client_valid(lc)) return;
	if(!lc->vc) return;
	/* Third: calculate the md5 */
	char *md5 = lwqq_enc_pwd(lc->password, lc->vc->str, lc->vc->uin);

	/* Last: do real login */
	LwqqAsyncEvent* ev = do_login(lc, md5, NULL);
	s_free(md5);
	lwqq_async_add_event_listener(ev,_C_(2p,login_stage_5,ev,ec));

}
static void login_stage_5(LwqqAsyncEvent* ev,LwqqErrorCode* ec)
{
	int err = ev->result;
	if(ec)(*ec=err);
	LwqqClient* lc = ev->lc;
	if(!lwqq_client_valid(lc)) return;
	/* Free old value */

	if(err != LWQQ_EC_OK){
		lc->stat = LWQQ_STATUS_LOGOUT;
		lc->args->login_ec = err;
		vp_do_repeat(lc->events->login_complete, NULL);
		return;
	}
	LwqqAsyncEvent* event = set_online_status(lc, lwqq_status_to_str(lc->stat));
	lwqq_async_add_event_listener(event,_C_(2p,login_stage_6,event,ec));
}

static void login_stage_6(LwqqAsyncEvent* ev, LwqqErrorCode* ec)
{
	int err = ev->result;
	if(ec)(*ec=err);
	LwqqClient* lc = ev->lc;
	if(!lwqq_client_valid(lc)) return;

	lwqq_vc_free(lc->vc);
	lc->vc = NULL;
	if(err) lc->stat = LWQQ_STATUS_LOGOUT;
	lc->args->login_ec = err;
	vp_do_repeat(lc->events->login_complete, NULL);
}

/** 
 * WebQQ logout function
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
LWQQ_EXPORT
LwqqErrorCode lwqq_logout(LwqqClient *client, unsigned wait_time)
{
	LwqqClient* lc = client;
	LwqqHttpRequest *req = NULL;  
	int ret;
	LwqqErrorCode err = LWQQ_EC_OK;
	json_t *json = NULL;
	char *value;
	struct timeval tv;
	long int re;
	char url[512];

	if (!client) {
		lwqq_log(LOG_ERROR, "Invalid pointer\n");
		return LWQQ_EC_NULL_POINTER;
	}

	/* Get the milliseconds of now */
	if (gettimeofday(&tv, NULL)) {
		return LWQQ_EC_ERROR;
	}
	re = tv.tv_usec / 1000;
	re += tv.tv_sec;

	snprintf(url, sizeof(url), "%s/channel/logout2"
			"?clientid=%s&psessionid=%s&t=%ld",
			WEBQQ_D_HOST, client->clientid, client->psessionid, re);

	/* Create a GET request */
	req = lwqq_http_create_default_request(client,url, NULL);
	if (!req) {
		goto done;
	}

	/* Set header needed by server */
	req->set_header(req, "Referer", WEBQQ_LOGIN_REF_URL);

	if(wait_time>0)
		lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT, wait_time); // this perform good when no network link
	req->retry = 0;
	ret = req->do_request(req, 0, NULL);
	if (ret) {
		lwqq_log(LOG_ERROR, "Send logout request failed\n");
		err = LWQQ_EC_NETWORK_ERROR;
		goto done;
	}
	if (req->http_code != 200) {
		err = LWQQ_EC_HTTP_ERROR;
		goto done;
	}

	lwqq__jump_if_json_fail(json, req->response, err);

	/* Check whether logout correctly */
	value = json_parse_simple_value(json, "retcode");
	if (!value || strcmp(value, "0")) {
		err = LWQQ_EC_ERROR;
		goto done;
	}
	value = json_parse_simple_value(json, "result");
	if (!value || strcmp(value, "ok")) {
		err = LWQQ_EC_ERROR;
		goto done;
	}

done:
	lwqq__clean_json_and_req(json, req);
	client->stat = LWQQ_STATUS_LOGOUT;
	return err;
}

static int process_login2(LwqqHttpRequest* req)
{
	/*
	 * {"retcode":0,"result":{"uin":2501542492,"cip":3396791469,"index":1075,"port":49648,"status":"online","vfwebqq":"8e6abfdb20f9436be07e652397a1197553f49fabd3e67fc88ad7ee4de763f337e120fdf7036176c9","psessionid":"8368046764001d636f6e6e7365727665725f77656271714031302e3133392e372e31363000003bce00000f8a026e04005c821a956d0000000a407646664c41737a42416d000000288e6abfdb20f9436be07e652397a1197553f49fabd3e67fc88ad7ee4de763f337e120fdf7036176c9","user_state":0,"f":0}}
	 */
	int err = 0;
	LwqqClient* lc = req->lc;
	json_t* root = NULL,*result;
	lwqq__jump_if_http_fail(req,err);
	lwqq__jump_if_json_fail(root,req->response,err);
	result = lwqq__parse_retcode_result(root, &err);
	switch(err){
		case 0:
			lwqq_puts("[ReLinkSuccess]");
			break;
		case 103:
			lwqq_puts("[Not Relogin]");
			vp_do_repeat(lc->events->poll_lost, NULL);
			goto done;
		case 113:
		case 115:
		case 112:
			lwqq_puts("[RelinkFailure]");
			vp_do_repeat(lc->events->poll_lost, NULL);
			goto done;
		default:
			lwqq_puts("[RelinkStop]");
			vp_do_repeat(lc->events->poll_lost, NULL);
			goto done;
	}
	if(result){
		lwqq_override(lc->cip,lwqq__json_get_value(result,"cip"));
		lwqq_override(lc->index,lwqq__json_get_value(result,"index"));
		lwqq_override(lc->port,lwqq__json_get_value(result,"port"));
		lwqq_override(lc->psessionid,lwqq__json_get_value(result,"psessionid"));
		lwqq_override(lc->vfwebqq,lwqq__json_get_value(result,"vfwebqq"));
		lc->stat = lwqq_status_from_str(json_parse_simple_value(result, "status"));
	}
	lwqq_override(lc->session.ptwebqq, lwqq_http_get_cookie(req, "ptwebqq"));
done:
	lwqq__log_if_error(err, req);
	lwqq__clean_json_and_req(root,req);
	return err;
}

LWQQ_EXPORT
LwqqAsyncEvent* lwqq_relink(LwqqClient* lc)
{
	if(!lc) return NULL;
	char url[128];
	char post[512];
	snprintf(url, sizeof(url), "%s/channel/login2",WEBQQ_D_HOST);
	LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
	if(!lc->new_ptwebqq)
		lc->new_ptwebqq = lwqq_http_get_cookie(req, "ptwebqq");
	snprintf(post, sizeof(post), "r={\"status\":\"%s\",\"ptwebqq\":\"%s\",\"passwd_sig\":\"\",\"clientid\":\"%s\",\"psessionid\":\"%s\"}",lwqq_status_to_str(lc->stat),lc->new_ptwebqq,lc->clientid,lc->psessionid);
	req->set_header(req,"Referer",WEBQQ_D_REF_URL);
	lwqq_http_set_cookie(req, "ptwebqq", lc->new_ptwebqq, 1);
	req->retry = 0;
	return req->do_request_async(req,lwqq__has_post(),_C_(p_i,process_login2,req));
}

// vim: ts=3 sw=3 sts=3 noet
