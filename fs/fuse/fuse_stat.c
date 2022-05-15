/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2020-2028

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "fuse_i.h"
#include "fuse_stat.h"

static void fuse_stat_get_status_work(struct work_struct *work);
static void fuse_stat_schedule_work(struct fuse_stat *stat);

static ssize_t fuse_stat_freq_request_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.freq_request);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_freq_request_ops = {
	.open = nonseekable_open,
	.read = fuse_stat_freq_request_read,
	.llseek = no_llseek,
};

static ssize_t fuse_stat_load_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.info.load);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_load_ops = {
	.open = nonseekable_open,
	.read = fuse_stat_load_read,
	.llseek = no_llseek,
};

static ssize_t fuse_is_primary_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.is_primary);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_is_primary_ops = {
	.open = nonseekable_open,
	.read = fuse_is_primary_read,
	.llseek = no_llseek,
};

static ssize_t fuse_load_scale_up_thre_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 100);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.load_scale_up_thre = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_load_scale_up_thre_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.load_scale_up_thre);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_load_scale_up_thre_ops = {
	.open = nonseekable_open,
	.read = fuse_load_scale_up_thre_read,
	.write = fuse_load_scale_up_thre_write,
	.llseek = no_llseek,
};

static ssize_t fuse_load_scale_down_thre_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 100);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.load_scale_down_thre = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_load_scale_down_thre_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.load_scale_down_thre);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_load_scale_down_thre_ops = {
	.open = nonseekable_open,
	.read = fuse_load_scale_down_thre_read,
	.write = fuse_load_scale_down_thre_write,
	.llseek = no_llseek,
};

static ssize_t fuse_delay_ms_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 1000);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.delay_ms = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_delay_ms_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.delay_ms);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_delay_ms_ops = {
	.open = nonseekable_open,
	.read = fuse_delay_ms_read,
	.write = fuse_delay_ms_write,
	.llseek = no_llseek,
};

static ssize_t fuse_load_warn_thre_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 100);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.load_warn_thre = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_load_warn_thre_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.load_warn_thre);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_load_warn_thre_ops = {
	.open = nonseekable_open,
	.read = fuse_load_warn_thre_read,
	.write = fuse_load_warn_thre_write,
	.llseek = no_llseek,
};

static ssize_t fuse_debug_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 1);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.debug = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_debug_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.debug);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_debug_ops = {
	.open = nonseekable_open,
	.read = fuse_debug_read,
	.write = fuse_debug_write,
	.llseek = no_llseek,
};

static ssize_t fuse_is_enabled_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	int uninitialized_var(val);
	ssize_t ret;

	ret = fuse_conn_limit_write(file, buf, count, ppos, &val, 1);
	if (ret > 0) {
		struct fuse_conn *fc = fuse_ctl_file_conn_get(file);
		if (fc) {
			fc->stat.is_enabled = val;
			fuse_conn_put(fc);
		}
	}

	return ret;
}

static ssize_t fuse_is_enabled_read(struct file *file,
						   char __user *buf, size_t len,
						   loff_t *ppos)
{
	struct fuse_conn *fc;
	unsigned val;

	fc = fuse_ctl_file_conn_get(file);
	if (!fc)
		return 0;

	val = READ_ONCE(fc->stat.is_enabled);
	fuse_conn_put(fc);

	return fuse_conn_limit_read(file, buf, len, ppos, val);
}

static const struct file_operations fuse_conn_is_enabled_ops = {
	.open = nonseekable_open,
	.read = fuse_is_enabled_read,
	.write = fuse_is_enabled_write,
	.llseek = no_llseek,
};

static struct dentry *fuse_ctl_add_file_dentry(struct dentry *parent,
		struct fuse_conn *fc,
		const char *name,
		int mode, int nlink,
		const struct file_operations *fop)
{
	struct dentry *ret;

	inc_nlink(d_inode(parent));
	ret = fuse_ctl_add_dentry(parent, fc, name, mode, nlink, NULL, fop);
	if (!ret)
		fuse_err(fc, "create %s failed\n", __func__, __LINE__, name);

	return ret;
}

int fuse_stat_ctl_add_conn(struct fuse_conn *fc, struct dentry *parent)
{
	fuse_ctl_add_file_dentry(parent, fc, "freq_request", S_IFREG | S_IRUGO, 1,
				     &fuse_conn_freq_request_ops);
	fuse_ctl_add_file_dentry(parent, fc, "load", S_IFREG | S_IRUGO, 1,
				     &fuse_conn_load_ops);
	fuse_ctl_add_file_dentry(parent, fc, "is_primary", S_IFREG | S_IRUGO, 1,
				     &fuse_conn_is_primary_ops);
	fuse_ctl_add_file_dentry(parent, fc, "load_scale_up_thre", S_IFREG | S_IRUGO, 1,
				     &fuse_conn_load_scale_up_thre_ops);
	fuse_ctl_add_file_dentry(parent, fc, "load_scale_down_thre", S_IFREG | S_IRUGO | S_IWUGO, 1,
				     &fuse_conn_load_scale_down_thre_ops);
	fuse_ctl_add_file_dentry(parent, fc, "delay_ms", S_IFREG | S_IRUGO | S_IWUGO, 1,
				     &fuse_conn_delay_ms_ops);
	fuse_ctl_add_file_dentry(parent, fc, "load_warn_thre", S_IFREG | S_IRUGO | S_IWUGO, 1,
				     &fuse_conn_load_warn_thre_ops);
	fuse_ctl_add_file_dentry(parent, fc, "is_enabled", S_IFREG | S_IRUGO | S_IWUGO, 1,
				     &fuse_conn_is_enabled_ops);
	fuse_ctl_add_file_dentry(parent, fc, "debug", S_IFREG | S_IRUGO | S_IWUGO, 1,
				     &fuse_conn_debug_ops);

	return 0;
}

void fuse_printk(struct fuse_conn *fc, const char *func, int line, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int level;

	va_start(args, fmt);

	level = printk_get_level(fmt);
	vaf.fmt = printk_skip_level(fmt);
	vaf.va = &args;
	printk("%c%c[%s:%d]fuse-fs(%d): %pV\n",
			KERN_SOH_ASCII, level, func, line, fc->dev, &vaf);

	va_end(args);
}

static inline int fuse_is_stat_supported(struct fuse_conn *fc)
{
	return fc->stat.is_enabled;
}

static int fuse_is_busy(struct fuse_conn *fc)
{
	// struct fuse_pqueue *fpq = &fud->pq;

	return !list_empty(&fc->iq.pending);
		// request_pending(&fc->iq);
		// || !list_empty(&fpq->io)
		// || !list_empty(&fpq->processing);
}

void fuse_start_busy(struct fuse_conn *fc)
{
	unsigned long flags;
	struct fuse_stat *stat;

	if (!fuse_is_stat_supported(fc))
		return;

	if (!fuse_is_busy(fc))
		return;

	stat = &fc->stat;
	spin_lock_irqsave(&stat->lock, flags);

	if (!stat->info.window_start_t) {
		stat->info.window_start_t = jiffies;
		stat->info.tot_busy_t = 0;
		stat->info.is_busy_started = false;

		// if stat->delayed_work is no working, start it here
		if (!delayed_work_pending(&stat->delayed_work)) {
			schedule_delayed_work(&stat->delayed_work, 0);
			/* fuse_debug(fc, "stat worker will be started."); */
		}
	}

	if (!stat->info.is_busy_started) {
		stat->info.busy_start_t = ktime_get();
		stat->info.is_busy_started = true;

		/* fuse_debug(fc, "stat start busy. num_waiting %d num_background %d" */
				/* " blocked %d dev_count %d pending: %d interrupts %d forget %d", */
				/* atomic_read(&fc->num_waiting), */
				/* fc->num_background, */
				/* fc->blocked, */
				/* fc->dev_count, */
				/* !list_empty(&fc->iq.pending), */
				/* !list_empty(&fc->iq.interrupts), */
				/* forget_pending(&fc->iq) */
				/* ); */
	}

	spin_unlock_irqrestore(&stat->lock, flags);

#if 0
	if (!list_empty(&fc->iq.pending)) {
		struct fuse_req *req;
		int i = 0;

		fuse_err(fc, "num_waiting %d num_background %d"
				" blocked %d dev_count %d pending: %d interrupts %d forget %d",
				atomic_read(&fc->num_waiting),
				fc->num_background,
				fc->blocked,
				fc->dev_count,
				!list_empty(&fc->iq.pending),
				!list_empty(&fc->iq.interrupts),
				forget_pending(&fc->iq)
				);

		list_for_each_entry(req, &fc->iq.pending, list) {
			pr_err("%d: opcode %d, ", i, req->in.h.opcode);
		}
		pr_err("\n");
	}
#endif
}

void fuse_update_busy(struct fuse_conn *fc)
{
	unsigned long flags;
	bool is_busy;
	struct fuse_stat *stat;

	if (!fuse_is_stat_supported(fc))
		return;

	is_busy = fuse_is_busy(fc);
	stat = &fc->stat;
	spin_lock_irqsave(&stat->lock, flags);
	if (!is_busy && stat->info.is_busy_started) {
		stat->info.tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					stat->info.busy_start_t));
		stat->info.busy_start_t = 0;
		stat->info.is_busy_started = false;
		/* fuse_debug(fc, "stat stop busy. num_waiting %d num_background %d" */
				/* " blocked %d dev_count %d pending: %d interrupts %d forget %d", */
				/* atomic_read(&fc->num_waiting), */
				/* fc->num_background, */
				/* fc->blocked, */
				/* fc->dev_count, */
				/* !list_empty(&fc->iq.pending), */
				/* !list_empty(&fc->iq.interrupts), */
				/* forget_pending(&fc->iq) */
				/* ); */
	}
	spin_unlock_irqrestore(&stat->lock, flags);
}

static void fuse_stat_schedule_work(struct fuse_stat *stat)
{
	unsigned long delay_in_jiffies;

	delay_in_jiffies = msecs_to_jiffies(stat->delay_ms);
	schedule_delayed_work(&stat->delayed_work, delay_in_jiffies);
}

static void fuse_stat_get_status_work(struct work_struct *work)
{
	struct fuse_conn *fc = container_of(work, struct fuse_conn, stat.delayed_work.work);
	unsigned long flags;
	unsigned long total_time;
	unsigned long busy_time;
	struct fuse_stat *stat;
	bool is_busy;
	int load;

	if (!fuse_is_stat_supported(fc))
		return ;

	is_busy = fuse_is_busy(fc);
	stat = &fc->stat;
	spin_lock_irqsave(&stat->lock, flags);
	if (!stat->info.window_start_t)
		goto start_window;

	if (stat->info.is_busy_started)
		stat->info.tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					stat->info.busy_start_t));

	total_time = jiffies_to_usecs((long)jiffies -
				(long)stat->info.window_start_t);
	busy_time = stat->info.tot_busy_t;
	load = (busy_time * 100) / total_time;
	stat->info.load = load;
	if (!load && !is_busy) {
		stat->info.idle_cnt++;
		stat->freq_request = FUSE_SCALE_DOWN;
	} else if (load > stat->load_scale_up_thre) {
		stat->freq_request = FUSE_SCALE_UP;
	} else if (load < stat->load_scale_down_thre) {
		stat->freq_request = FUSE_SCALE_DOWN;
	} else
		stat->freq_request = FUSE_SCALE_NORMAL;

	if (fc->num_background > fc->congestion_threshold) // fuse should be congested or blocked.
		stat->freq_request = FUSE_SCALE_UP;

start_window:
	stat->info.window_start_t = jiffies;
	stat->info.tot_busy_t = 0;

	if (is_busy) {
		stat->info.busy_start_t = ktime_get();
		stat->info.is_busy_started = true;
	} else {
		stat->info.busy_start_t = 0;
		stat->info.is_busy_started = false;
	}
	spin_unlock_irqrestore(&stat->lock, flags);

	fuse_debug(fc, "load %d freq_request %d num_waiting %d num_background %d"
			" blocked %d dev_count %d pending: %d interrupts %d forget %d",
			load,
			stat->freq_request,
			atomic_read(&fc->num_waiting),
			fc->num_background,
			fc->blocked,
			fc->dev_count,
			!list_empty(&fc->iq.pending),
			!list_empty(&fc->iq.interrupts),
			forget_pending(&fc->iq)
			);
	if (stat->load_warn_thre) {
		if (load > stat->load_warn_thre) {
			stat->info.load_high_cnt++;
			if ((stat->info.load_high_cnt >= FUSE_STAT_LOAD_HIGH_COUNTER_THRESHOLD)
					&& __ratelimit(&stat->ratelimit)) {
				fuse_warn(fc, "loading is high: %d%% num_waiting %d num_background %d"
						" blocked %d dev_count %d pending: %d interrupts %d forget %d",
						load,
						atomic_read(&fc->num_waiting),
						fc->num_background,
						fc->blocked,
						fc->dev_count,
						!list_empty(&fc->iq.pending),
						!list_empty(&fc->iq.interrupts),
						forget_pending(&fc->iq)
						);
				stat->info.load_high_cnt = 0;
			}
		} else
			stat->info.load_high_cnt = 0;
	}

	if (stat->info.idle_cnt < FUSE_STAT_IDLE_MAX) {
		fuse_stat_schedule_work(stat);
	} else {
		fuse_debug(fc, "stat worker is stopped. idle_cnt %d", stat->info.idle_cnt);
		stat->info.window_start_t = 0;
		stat->info.idle_cnt = 0;
	}

	return ;
}

static void fuse_stat_reset(struct fuse_stat *stat)
{
	memset(stat, 0, sizeof(*stat));
}

int fuse_stat_init(struct fuse_conn *fc)
{
	struct fuse_stat *stat = &fc->stat;

	pr_err("[%s:%d] enter (dev %d)\n", __func__, __LINE__, fc->dev);

	fuse_stat_reset(stat);
	spin_lock_init(&stat->lock);
	stat->delay_ms = FUSE_STAT_INTERVAL_MS;
	INIT_DELAYED_WORK(&stat->delayed_work, fuse_stat_get_status_work);
	// fuse_stat_schedule_work(stat);
	stat->load_warn_thre = FUSE_STAT_LOAD_THRESHOLD;
	stat->load_scale_up_thre = FUSE_STAT_LOAD_SCALE_UP_THRESHOLD;
	stat->load_scale_down_thre = FUSE_STAT_LOAD_SCALE_DOWN_THRESHOLD;

	/*  Not more than 5 times every 3 seconds.  */
	ratelimit_state_init(&stat->ratelimit,
						FUSE_STAT_DEFAULT_RATELIMIT_INTERVAL, 5);
	stat->debug = false;
	stat->is_enabled = false;

	stat->is_primary = 1; // TODO:

	return 0;
}

int fuse_stat_exit(struct fuse_conn *fc)
{
	pr_err("[%s:%d] enter (dev %d)\n", __func__, __LINE__, fc->dev);

	fc->stat.freq_request = FUSE_SCALE_DOWN;
	fc->stat.is_enabled = false;
	cancel_delayed_work_sync(&fc->stat.delayed_work);
	return 0;
}
