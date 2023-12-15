#include <stdlib.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <stddef.h>

#include <linux/virtio_config.h>
#include <linux/virtio_gpu.h>
#include <linux/virtio_ids.h>

#include "libvduse.h"
#include "epoll_scheduler.h"

#define TRACE_FILE "test_vduse_gpu.c"
#include "trace.h"

#define DRIVER_NAME "test_gpu"
#define QUEUE_SIZE 16

#define QUEUE_CMD    0
#define QUEUE_CURSOR 1
#define QUEUE_MAX    2

#define PCI_VENDOR_ID_REDHAT_QUMRANET 0x1af4

struct queue {
	struct es_thread thread;
	bool enabled;
	VduseVirtq *vq;
};

#define container_of(ptr, type, member) ({                    \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);  \
	(type *)( (char *)__mptr - offsetof(type,member) );})

static inline struct queue *thread_to_queue(struct es_thread *th)
{
	return container_of(th, struct queue, thread);
}

static struct es *es;
static VduseDev *dev;

static enum es_test_result dev_test(struct es_thread *self)
{
	(void)self;

	return ES_WAIT;
}

static int dev_go(struct es_thread *self, uint32_t events)
{
	(void)self;
	(void)events;

	int err = vduse_dev_handler(dev);
	if (err < 0) {
		trace_err("vduse_dev_handler()");
		return err;
	}

	return 0;
}

static enum es_test_result cmd_test(struct es_thread *self)
{
	struct queue *q = thread_to_queue(self);
	if (!q->enabled)
		return ES_EXIT_THREAD;

	return ES_WAIT;
}

static int cmd_go(struct es_thread *self, uint32_t events)
{
	(void)events;

	struct queue *q = thread_to_queue(self);
	if (!q->enabled)
		return 0;

	eventfd_t ev;
	int ret = eventfd_read(self->fd, &ev);
	if (ret < 0) {
		trace_err("eventfd_read()");
		return -1;
	}

	while (1) {
		VduseVirtqElement *req = vduse_queue_pop(q->vq, sizeof(VduseVirtqElement));
		if (!req)
			break;

		trace_err("out_num=%u, in_num=%u", req->out_num, req->in_num);

		vduse_queue_push(q->vq, req, 0);
		free(req);
	}

	vduse_queue_notify(q->vq);

	return 0;
}

static enum es_test_result cursor_test(struct es_thread *self)
{
	struct queue *q = thread_to_queue(self);
	if (!q->enabled)
		return ES_EXIT_THREAD;

	return ES_WAIT;
}

static int cursor_go(struct es_thread *self, uint32_t events)
{
	(void)events;

	struct queue *q = thread_to_queue(self);
	if (!q->enabled)
		return 0;

	eventfd_t ev;
	int ret = eventfd_read(self->fd, &ev);
	if (ret < 0) {
		trace_err("eventfd_read()");
		return -1;
	}

	while (1) {
		VduseVirtqElement *req = vduse_queue_pop(q->vq, sizeof(VduseVirtqElement));
		if (!req)
			break;

		trace_err("out_num=%u, in_num=%u", req->out_num, req->in_num);

		vduse_queue_push(q->vq, req, 0);
		free(req);
	}

	vduse_queue_notify(q->vq);

	return 0;
}

static struct es_thread timer_thread;

static enum es_test_result timer_test(struct es_thread *self)
{
	(void)self;
	return ES_WAIT;
}
static struct es_thread dev_thread;
static int timer_go(struct es_thread *self, uint32_t events)
{
	(void)self;
	(void)events;

	static unsigned int n = 0;

	eventfd_t ev;
	int ret = eventfd_read(self->fd, &ev);
	if (ret < 0) {
		trace_err("eventfd_read()");
		return -1;
	}

	n++;
	trace("%d", n);

	return 0;
}

static struct es_thread dev_thread = {
	.name   = "dev",
	.events = EPOLLIN,
	.test   = dev_test,
	.go     = dev_go,
	.done   = NULL,
};

static struct queue queues[QUEUE_MAX] = {
	[QUEUE_CMD] = {
		.thread = {
			.name   = "queue_cmd",
			.events = EPOLLIN,
			.test   = cmd_test,
			.go     = cmd_go,
			.done   = NULL,
		},
	},
	[QUEUE_CURSOR] = {
		.thread = {
			.name   = "queue_cursor",
			.events = EPOLLIN,
			.test   = cursor_test,
			.go     = cursor_go,
			.done   = NULL,
		},
	},
};
static struct es_thread timer_thread = {
	.name   = "timer",
	.events = EPOLLIN,
	.test   = timer_test,
	.go     = timer_go,
	.done   = NULL,
};

static void test_enable_queue(VduseDev *dev, VduseVirtq *vq)
{
	(void)dev;
	(void)vq;

	int index = vduse_queue_get_index(vq);

	if (index < 0 || QUEUE_MAX <= index) {
		trace_err("index == %d", index);
		return;
	}

	struct queue *q = queues + index;

	q->thread.fd = vduse_queue_get_fd(vq);
	q->vq = vq;
	q->enabled = true;

	trace("%s, fd=%d", q->thread.name, q->thread.fd);

	int err = es_add(es, &q->thread);
	if (err)
		trace_err("es_add(\"%s\")", q->thread.name);
}

static void test_disable_queue(VduseDev *dev, VduseVirtq *vq)
{
	(void)dev;

	int index = vduse_queue_get_index(vq);

	if (index < 0 || QUEUE_MAX <= index) {
		trace_err("index == %d", index);
		return;
	}

	struct queue *q = queues + index;

	trace("%s", q->thread.name);

	q->enabled = false;
}

static const VduseOps ops = {
	.enable_queue  = test_enable_queue,
	.disable_queue = test_disable_queue,
};

int main(int argc, char **argv)
{
	int err;
	uint64_t driver_features = vduse_get_virtio_features() | VIRTIO_GPU_F_VIRGL;

	(void)argc;
	(void)argv;

	trace("hello");

	/* vduse */

	struct virtio_gpu_config dev_cfg = {
		.events_read  = 0,
		.events_clear = 0,
		.num_scanouts = 1,
		.num_capsets  = 1,
	};

	dev = vduse_dev_create(DRIVER_NAME, VIRTIO_ID_GPU, PCI_VENDOR_ID_REDHAT_QUMRANET,
					 driver_features, QUEUE_MAX,
					 sizeof(struct virtio_gpu_config), (char *)&dev_cfg, &ops, NULL);
	if (!dev) {
		trace_err("vduse_dev_create()");
		goto error;
	}
	dev_thread.fd = vduse_dev_get_fd(dev);
	trace("dev fd=%d", dev_thread.fd);

	err = vduse_set_reconnect_log_file(dev, "/tmp/vduse-" DRIVER_NAME ".log");
	if (err) {
		trace_err("vduse_set_reconnect_log_file()");
		goto error_dev_destroy;
	}

	for (int i = 0; i < QUEUE_MAX; i++) {
		err = vduse_dev_setup_queue(dev, i, QUEUE_SIZE);
		if (err) {
			trace_err("vduse_dev_setup_queue(%d, %d)", i, QUEUE_SIZE);
			goto error_dev_destroy;
		}
	}

	/* timer */

	err = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (err < 0) {
		trace_err_p("timerfd_create()");
		goto error_dev_destroy;
	}
	timer_thread.fd = err;

	struct itimerspec t = {
		.it_interval = {
			.tv_sec = 5,
			.tv_nsec = 0,
		},
		.it_value = {
			.tv_sec = 5,
			.tv_nsec = 0,
		},
	};

	err = timerfd_settime(timer_thread.fd, 0, &t, NULL);
	if (err < 0) {
		trace_err_p("timerfd_settime()");
		goto error_close_timer_fd;
	}

	/* schedule */

	es = es_init(
		&dev_thread,
		&timer_thread,
		NULL);
	if (!es) {
		trace_err("es_init()");
		goto error_close_timer_fd;
	}

	err = es_schedule(es);
	if (err < 0) {
		trace_err("es_schedule()");
		goto error_close_timer_fd;
	}

	trace("done");

	close(timer_thread.fd);
	vduse_dev_destroy(dev);

	trace("exit");
	exit(EXIT_SUCCESS);

error_close_timer_fd:
	close(timer_thread.fd);
error_dev_destroy:
	vduse_dev_destroy(dev);
error:
	trace_err("exit failure");
	exit(EXIT_FAILURE);
}
