#include "epoll_scheduler.h"
#define TRACE_FILE "epoll_scheduler.c"
#include "trace.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <assert.h>

#define INITIAL_CAPACITY 16
struct es {
	struct es_thread **data; // FIXME rename to threads
	struct epoll_event *events;
	unsigned int data_len;
	unsigned int data_capacity;
	int epoll_fd;
};

static void es_done(struct es *es)
{
	unsigned int n;

	assert(es);

	for (n = 0; n < es->data_len; n++) {
		struct es_thread *th = es->data[n];
		if (!th)
			continue;
		epoll_ctl(es->epoll_fd, EPOLL_CTL_DEL, th->fd, (void *)1);
		if (th->done)
			th->done(th);
	}

	close(es->epoll_fd);

	free(es->data);
	free(es->events);
	free(es);
}

static int es_resize(struct es *es)
{
	unsigned int capacity_new = es->data_capacity * 2;

	struct es_thread **data_new = reallocarray(es->data, capacity_new, sizeof(struct es_thread *));
	if (!data_new) {
		trace_err_p("reallocarray() data");
		return -1;
	}
	es->data = data_new;

	struct epoll_event *events_new = reallocarray(es->events, capacity_new, sizeof(struct epoll_event));
	if (!events_new) {
		trace_err_p("reallocarray() events");
		return -1;
	}
	es->events = events_new;

	es->data_capacity = capacity_new;

	return 0;
}

static int _es_add(struct es *es, struct es_thread *th)
{
	int ret;

	assert(th->fd >= 0);
	assert(th->test);
	assert(th->name);

	th->private.data.u32 = es->data_len;
	th->private.events = 0;

	trace("epoll_ctl(ADD) n=%u, epoll_fd=%d, fd=%d", es->data_len, es->epoll_fd, th->fd);
	ret = epoll_ctl(es->epoll_fd, EPOLL_CTL_ADD, th->fd, &th->private);
	if (ret < 0) {
		trace_err_p("epoll_ctl(ADD) n=%u", es->data_len);
		return ret;
	}

	es->data[es->data_len++] = th;
	return ret;
}

struct es *es_init(struct es_thread *thread, ...)
{
	unsigned int threads;
	unsigned int capacity;
	struct es_thread *th;
	va_list va1, va2;
	int err;

	assert(thread);

	va_start(va1, thread);
	va_copy(va2, va1);

	for (threads = 1; va_arg(va1, void *); threads++)
		;

	struct es *es = malloc(sizeof(struct es));
	if (!es) {
		trace_err("malloc() es");
		goto error_va_end;
	}

	es->epoll_fd = epoll_create(1);
	if (es->epoll_fd < 0) {
		trace_err_p("epoll_create()");
		goto error_free;
	}

	for (capacity = INITIAL_CAPACITY; capacity < threads; capacity *= 2)
		;

	es->data_len = 0;
	es->data_capacity = capacity;

	es->data = calloc(capacity, sizeof(struct es_data *));
	if (!es->data) {
		trace_err("calloc() data");
		goto error_close;
	}

	err = _es_add(es, thread);
	if (err < 0) {
		trace_err("_es_add(\'%s\')", thread->name);
		goto error_free_data;
	}

	for (th = va_arg(va2, struct es_thread *); th; th = va_arg(va2, struct es_thread *)) {
		// trace("fd=%d, test=0x%lu, name=\'%s\'", th->fd, (long unsigned int)th->test, th->name);
		err = _es_add(es, th);
		if (err < 0) {
			trace_err("_es_add(\'%s\')", th->name);
			goto error_epoll_ctl_del;
		}
	}

	es->events = calloc(capacity, sizeof(struct epoll_event));
	if (!es->events) {
		trace_err("calloc() events");
		goto error_epoll_ctl_del;
	}

	trace("new scheduler: len=%u, capacity=%u", es->data_len, es->data_capacity);

	va_end(va1);
	va_end(va2);

	return es;

error_epoll_ctl_del: {
		unsigned int i;
		for (i = 0; i < es->data_len; i++)
			epoll_ctl(es->epoll_fd, EPOLL_CTL_DEL, es->data[i]->fd, (void *)1);
	}
error_free_data:
	free(es->data);
error_close:
	close(es->epoll_fd);
error_free:
	free(es);
error_va_end:
	va_end(va1);
	va_end(va2);
	return NULL;;
}

int es_add(struct es *es, struct es_thread *thread)
{
	int ret;

	assert(es);
	assert(thread);

	if (es->data_len == es->data_capacity) {
		ret = es_resize(es);
		if (ret < 0) {
			trace_err("es_resize()");
			es_done(es);
			return ret;
		}
	}

	ret = _es_add(es, thread);
	if (ret < 0) {
		trace_err_p("_es_add(\'%s\')", thread->name);
		es_done(es);
		return ret;
	}

	trace("new thread: name=%s", thread->name);
	return 0;
}

int es_schedule(struct es *es)
{
	unsigned int n;
	int i;
	int ret = 0;

	assert(es);
	assert(es->data_len);

	while (es->data_len) {

		unsigned int data_len = es->data_len;

		bool ready = false;
		unsigned int deleted = 0;

		for (n = 0; n < data_len; n++) {
			struct es_thread *th = es->data[n];
			ret = th->test(th);
			th->ready = false;
			if (ret == ES_DONE || ret < 0) {
				if (ret < 0)
					trace_err("\"%s\" test reported error", th->name);
				else
					trace("\"%s\" requested shutdown", th->name);
				goto done;
			} else if (ret == ES_EXIT_THREAD) {
				ret = epoll_ctl(es->epoll_fd, EPOLL_CTL_DEL, th->fd, (void *)1);
				if (ret < 0) {
					trace_err_p("epoll_ctl(DEL) (\"%s\")", th->name);
					goto done;
				}
				trace("\"%s\" is quitting", th->name);
				if (th->done)
					th->done(th);
				es->data[n] = NULL;
				deleted++;
			} else if (ret == ES_READY) {
				th->ready = true;
				ready = true;
			} else {
				assert(ret == ES_WAIT);
				if (th->events != th->private.events || n != th->private.data.u32 ) {
					th->private.events = th->events;
					th->private.data.u32 = n;
					ret = epoll_ctl(es->epoll_fd, EPOLL_CTL_MOD, th->fd, &th->private);
					if (ret < 0) {
						trace_err_p("epoll_ctl(MOD) (\"%s\")", th->name);
						goto done;
					}
				}
			}
		}

		ret = epoll_wait(es->epoll_fd, es->events, es->data_capacity, ready ? 0 : -1);
		if (ret < 0) {
			trace_err_p("epoll_wait()");
			goto done;
		} else if ( (!ready && ret == 0) || (unsigned int)ret >= es->data_capacity) {
			trace_err("epoll_wait() ???");
			ret = -1;
			goto done;
		}

		for (i = 0; i < ret; i++) {
			struct epoll_event *ev = es->events + i;
			struct es_thread *th = es->data[ev->data.u32];
			if (th->go) {
				ret = th->go(th, ev->events);
				if (ret < 0) {
					trace_err("\"%s\" go reported error @2", th->name);
					goto done;
				}
			}
			th->ready = false;
		}

		if (ready) {
			for (n = 0; n < data_len; n++) {
				struct es_thread *th = es->data[n];
				if (!th->ready)
					continue;
				if (th->go) {
					ret = th->go(th, 0);
					if (ret < 0) {
						trace_err("\"%s\" go reported error @3", th->name);
						goto done;
					}
				}
			}
		}

		if (deleted) {
			unsigned int from, to;

			for (from = to = 0; from < data_len; from++) {
				if (!es->data[from])
					continue;
				es->data[to] = es->data[from];
				to++;
			}

			assert(deleted == from - to);

			// check if some threads were added
			for (from = data_len; from < es->data_len; to++, from++) {
				assert(es->data[from]);
				es->data[to] = es->data[from];
			}

			es->data_len = to;
			ret = 0; // if data_len == 0 and we are quitting
		}
	}

done:
	es_done(es);
	return ret;
}
