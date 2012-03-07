#define EIO_REQ_MEMBERS \
  zval *callback;       \
  zval *zfd;           \
  int buflen;           \
  char *buf;

#include "libeio/eio.h"
#include "libeio/eio.c"

/*
 * Stores callback, fd and increases their refcount.
 * buffer will be assigned to the req->buf, and bufferlen
 * to req->buflen. If buflen != 0 then buf will be freed in
 * req_done().
 */
#define STORE_REQ(buffer, bufferlen)           \
	do { /* Save callback and file descriptor, \
	   freed in eio_func_done() */             \
	zval_add_ref(&callback);                  \
	zval_add_ref(fd);                          \
	/* Increase refcount on the loop, to prevent it from exiting while still waiting \
	   for calls to be finished. Coupled with an ev_unref() in req_done() */ \
	ev_ref(eio_loop);                          \
	                                           \
	req->buflen   = bufferlen;                 \
	req->buf      = buffer;                    \
	req->callback = callback;                  \
	req->zfd      = *fd; } while(0)


zend_class_entry *eio_ce;

static struct ev_loop  *eio_loop = NULL;
static struct ev_idle  eio_poll_watcher;
static struct ev_async eio_ready_watcher;


static void eio_repeat_poll(struct ev_loop *loop, ev_idle *w, int revents)
{
	int i = eio_poll();
	
	if(i != -1)
	{
		IF_DEBUG(libev_printf("stopping eio_poll_watcher()\n"));
		ev_idle_stop(loop, w);
	}
	libev_printf("eio_poll(): %d\n", i);
}

static void eio_ready(struct ev_loop *loop, ev_idle *w, int revents)
{
	IF_DEBUG(libev_printf("eio_ready() "));
	int i = eio_poll();
	
	if(i == -1)
	{
		IF_DEBUG(php_printf("starting eio_poll_watcher"));
		ev_idle_start(eio_loop, &eio_poll_watcher);
	}
	IF_DEBUG(php_printf("\n"));
	libev_printf("eio_poll(): %d\n", i);
}

static void eio_want_poll()
{
	IF_DEBUG(libev_printf("eio_want_poll()\n"));
	ev_async_send(eio_loop, &eio_ready_watcher);
}


PHP_METHOD(EIO, init)
{
	if(eio_loop)
	{
		RETURN_BOOL(0);
	}
	
	/* TODO: Allow any other loop? */
	eio_loop = EV_DEFAULT;
	
	ev_idle_init(&eio_poll_watcher, eio_repeat_poll);
	ev_async_init(&eio_ready_watcher, eio_ready);
	ev_async_start(eio_loop, &eio_ready_watcher);
	ev_unref(eio_loop);
	
	eio_init(eio_want_poll, 0);
	
	RETURN_BOOL(1);
}

static int req_done(eio_req *req)
{
	zval *args[2];
	zval retval;
	
	MAKE_STD_ZVAL(args[0]);
	MAKE_STD_ZVAL(args[1]);
	
	ZVAL_LONG(args[1], req->errorno);
	
	switch(req->type)
	{
		case EIO_READ:
			if(req->result == -1)
			{
				ZVAL_BOOL(args[0], 0);
			}
			else
			{
				ZVAL_STRINGL(args[0], req->buf, req->result, 1);
			}
			break;
		
		default:
			ZVAL_LONG(args[0], req->result);
	}
	
	if(call_user_function(EG(function_table), NULL, req->callback,
		&retval, 2, args TSRMLS_CC) == SUCCESS)
	{
		zval_dtor(&retval);
	}
	
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	
	zval_ptr_dtor(&req->callback);
	zval_ptr_dtor(&req->zfd);
	
	if(req->buflen)
	{
		efree(req->buf);
	}
	
	ev_unref(eio_loop);
	
	return 0;
}

PHP_METHOD(EIO, write)
{
	dFILE_DESC;
	dCALLBACK;
	char *string;
	int  string_len;
	eio_req *req;
	
	if( ! eio_loop)
	{
		zend_throw_exception(NULL, "libev\\EIO: EIO not initialized", 1 TSRMLS_DC);
		
		return;
	}
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Zsz", &fd, &string, &string_len, &callback) != SUCCESS) {
		return;
	}
	
	EXTRACT_FILE_DESC(EIO, write);
	
	CHECK_CALLBACK;
	
	req = eio_write((int) file_desc, string, string_len,
		/* offset */ 0, /* EIO pri */ 0, req_done, NULL);
	
	STORE_REQ(NULL, 0);
	
	/* TODO: Code for handling errors, ie. res == 0 */
	assert(req);
}

PHP_METHOD(EIO, read)
{
	dFILE_DESC;
	dCALLBACK;
	int len;
	char *string;
	eio_req *req;
	
	if( ! eio_loop)
	{
		zend_throw_exception(NULL, "libev\\EIO: EIO not initialized", 1 TSRMLS_DC);
		
		return;
	}
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Zlz", &fd, &len, &callback) != SUCCESS) {
		return;
	}
	
	EXTRACT_FILE_DESC(EIO, read);
	
	CHECK_CALLBACK;
	
	string = emalloc(len);
	
	req = eio_read((int) file_desc, string, len,
		/* offset */ 0, /* EIO pri */ 0, req_done, NULL);
	
	STORE_REQ(string, len);
	
	/* TODO: Code for handling errors, ie. res == 0 */
	assert(req);
}

static const zend_function_entry eio_methods[] = {
	ZEND_ME(EIO, init, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(EIO, write, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(EIO, read, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};













