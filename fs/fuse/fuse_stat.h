/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2020-2028

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifndef _FUSE_STAT_H
#define _FUSE_STAT_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/uio.h>
#include <linux/miscdevice.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/pipe_fs_i.h>
#include <linux/swap.h>
#include <linux/splice.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/workqueue.h>

struct fuse_stat_info {
	unsigned long tot_busy_t;
	unsigned long window_start_t;
	ktime_t busy_start_t;
	bool is_busy_started;
	bool is_scaled_up;
	int load;
	int load_high_cnt;
	int idle_cnt;
};

struct fuse_stat {
	spinlock_t lock;
	struct fuse_stat_info info;
	struct delayed_work delayed_work;

	bool is_enabled;
	bool is_primary;
	int freq_request;
	struct ratelimit_state	ratelimit;

	int delay_ms;
	int load_scale_up_thre;
	int load_scale_down_thre;
	int load_warn_thre;
	bool debug;
};

#define		FUSE_STAT_INTERVAL_MS					100
#define		FUSE_STAT_IDLE_MAX						3
#define		FUSE_STAT_LOAD_THRESHOLD				90
#define		FUSE_STAT_LOAD_SCALE_UP_THRESHOLD		60
#define		FUSE_STAT_LOAD_SCALE_DOWN_THRESHOLD		30
#define		FUSE_STAT_LOAD_HIGH_COUNTER_THRESHOLD	5
#define		FUSE_STAT_DEFAULT_RATELIMIT_INTERVAL	(3 * HZ)

typedef enum{
	FUSE_SCALE_NORMAL	= 0,
	FUSE_SCALE_UP		= 1,
	FUSE_SCALE_DOWN		= 2,
} cmd_types;

struct fuse_dev;
struct fuse_conn;
struct fuse_iqueue;

struct dentry *fuse_ctl_add_dentry(struct dentry *parent,
					  struct fuse_conn *fc,
					  const char *name,
					  int mode, int nlink,
					  const struct inode_operations *iop,
					  const struct file_operations *fop);
struct fuse_conn *fuse_ctl_file_conn_get(struct file *file);
ssize_t fuse_conn_limit_read(struct file *file, char __user *buf,
				    size_t len, loff_t *ppos, unsigned val);
ssize_t fuse_conn_limit_write(struct file *file, const char __user *buf,
				     size_t count, loff_t *ppos, unsigned *val,
				     unsigned global_limit);
int request_pending(struct fuse_iqueue *fiq);
int forget_pending(struct fuse_iqueue *fiq);

#ifdef CONFIG_FUSE_STATISTICS

int fuse_stat_init(struct fuse_conn *fc);
int fuse_stat_exit(struct fuse_conn *fc);
void fuse_start_busy(struct fuse_conn *fc);
void fuse_update_busy(struct fuse_conn *fc);
int fuse_stat_ctl_add_conn(struct fuse_conn *fc, struct dentry *parent);

void fuse_printk(struct fuse_conn *fc, const char *func, int line, const char *fmt, ...);

#else

static inline int fuse_stat_init(struct fuse_conn *fc)
{
	return 0;
}

static inline int fuse_stat_exit(struct fuse_conn *fc)
{
	return 0;
}

static inline void fuse_start_busy(struct fuse_conn *fc)
{
}

static inline void fuse_update_busy(struct fuse_conn *fc)
{
}

static inline int fuse_stat_ctl_add_conn(struct fuse_conn *fc, struct dentry *parent)
{
	return 0;
}

static inline void fuse_printk(struct fuse_conn *fc, const char *func, int line, const char *fmt, ...)
{
}

#endif

#define fuse_err(fc, fmt, ...)                                                 \
		fuse_printk(fc, __func__, __LINE__, KERN_ERR fmt, ##__VA_ARGS__)
#define fuse_warn(fc, fmt, ...)                                                \
		fuse_printk(fc, __func__, __LINE__, KERN_WARNING fmt, ##__VA_ARGS__)
#define fuse_notice(fc, fmt, ...)                                              \
		fuse_printk(fc, __func__, __LINE__, KERN_NOTICE fmt, ##__VA_ARGS__)
#define fuse_info(fc, fmt, ...)                                                \
		fuse_printk(fc, __func__, __LINE__, KERN_INFO fmt, ##__VA_ARGS__)
#define fuse_debug(fc, fmt, ...) do {                                          \
		if (fc->stat.debug)                                                    \
			fuse_printk(fc, __func__, __LINE__, KERN_DEBUG fmt, ##__VA_ARGS__); \
	} while (0)

#endif
