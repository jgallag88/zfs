/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2014 by Delphix. All rights reserved.
 */

#include <sys/procfs_list.h>
#include <sys/zfs_context.h>
#include <sys/zfs_debug_impl.h>

procfs_list_t zfs_dbgmsgs;
int zfs_dbgmsg_size = 0;
int zfs_dbgmsg_maxsize = 4<<20; /* 4MB */

/*
 * Internal ZFS debug messages are enabled by default.
 *
 * # Print debug messages
 * cat /proc/spl/kstat/zfs/dbgmsg
 *
 * # Disable the kernel debug message log.
 * echo 0 > /sys/module/zfs/parameters/zfs_dbgmsg_enable
 *
 * # Clear the kernel debug message log.
 * echo 0 >/proc/spl/kstat/zfs/dbgmsg
 */
int zfs_dbgmsg_enable = 1;

static int
zfs_dbgmsg_show_header(struct seq_file *f)
{
	seq_printf(f, "%-12s %-8s\n", "timestamp", "message");
	return (0);
}

static int
zfs_dbgmsg_show(struct seq_file *f, void *p)
{
	zfs_dbgmsg_t *zdm = (zfs_dbgmsg_t *)p;
	seq_printf(f, "%-12llu %-s\n",
	    (u_longlong_t)zdm->zdm_timestamp, zdm->zdm_msg);
	return (0);
}

static void
zfs_dbgmsg_purge(int max_size)
{
	zfs_dbgmsg_t *zdm;
	int size;

	while (zfs_dbgmsg_size > max_size) {
		zdm = list_remove_head(&zfs_dbgmsgs.pl_list);
		if (zdm == NULL)
			return;

		size = zdm->zdm_size;
		kmem_free(zdm, size);
		zfs_dbgmsg_size -= size;
	}
}

static int
zfs_dbgmsg_clear(procfs_list_t *procfs_list)
{
	mutex_enter(&zfs_dbgmsgs.pl_lock);
	zfs_dbgmsg_purge(0);
	mutex_exit(&zfs_dbgmsgs.pl_lock);
	return (0);
}

void
zfs_dbgmsg_init(void)
{
	procfs_list_install("zfs",
	    "dbgmsg",
	    &zfs_dbgmsgs,
	    zfs_dbgmsg_show,
	    zfs_dbgmsg_show_header,
	    zfs_dbgmsg_clear,
	    offsetof(zfs_dbgmsg_t, zdm_node));
}

void
zfs_dbgmsg_fini(void)
{
	procfs_list_uninstall(&zfs_dbgmsgs);
	zfs_dbgmsg_purge(0);
	procfs_list_destroy(&zfs_dbgmsgs);
}

void
__zfs_dbgmsg(char *buf)
{
	zfs_dbgmsg_t *zdm;
	int size;

	size = sizeof (zfs_dbgmsg_t) + strlen(buf);
	zdm = kmem_zalloc(size, KM_SLEEP);
	zdm->zdm_size = size;
	zdm->zdm_timestamp = gethrestime_sec();
	strcpy(zdm->zdm_msg, buf);

	mutex_enter(&zfs_dbgmsgs.pl_lock);
	procfs_list_add(&zfs_dbgmsgs, zdm);
	zfs_dbgmsg_size += size;
	zfs_dbgmsg_purge(MAX(zfs_dbgmsg_maxsize, 0));
	mutex_exit(&zfs_dbgmsgs.pl_lock);
}

void
__set_error(const char *file, const char *func, int line, int err)
{
	/*
	 * To enable this:
	 *
	 * $ echo 512 >/sys/module/zfs/parameters/zfs_flags
	 */
	if (zfs_flags & ZFS_DEBUG_SET_ERROR)
		__dprintf(file, func, line, "error %lu", err);
}

#ifdef _KERNEL
void
__dprintf(const char *file, const char *func, int line, const char *fmt, ...)
{
	const char *newfile;
	va_list adx;
	size_t size;
	char *buf;
	char *nl;
	int i;

	size = 1024;
	buf = kmem_alloc(size, KM_SLEEP);

	/*
	 * Get rid of annoying prefix to filename.
	 */
	newfile = strrchr(file, '/');
	if (newfile != NULL) {
		newfile = newfile + 1; /* Get rid of leading / */
	} else {
		newfile = file;
	}

	i = snprintf(buf, size, "%s:%d:%s(): ", newfile, line, func);

	if (i < size) {
		va_start(adx, fmt);
		(void) vsnprintf(buf + i, size - i, fmt, adx);
		va_end(adx);
	}

	/*
	 * Get rid of trailing newline.
	 */
	nl = strrchr(buf, '\n');
	if (nl != NULL)
		*nl = '\0';

	/*
	 * To get this data enable the zfs__dprintf trace point as shown:
	 *
	 * # Enable zfs__dprintf tracepoint, clear the tracepoint ring buffer
	 * $ echo 1 > /sys/kernel/debug/tracing/events/zfs/enable
	 * $ echo 0 > /sys/kernel/debug/tracing/trace
	 *
	 * # Dump the ring buffer.
	 * $ cat /sys/kernel/debug/tracing/trace
	 */
	DTRACE_PROBE1(zfs__dprintf, char *, buf);

	/*
	 * To get this data:
	 *
	 * $ cat /proc/spl/kstat/zfs/dbgmsg
	 *
	 * To clear the buffer:
	 * $ echo 0 > /proc/spl/kstat/zfs/dbgmsg
	 */
	__zfs_dbgmsg(buf);

	kmem_free(buf, size);
}

#else

void
zfs_dbgmsg_print(const char *tag)
{

	(void) printf("ZFS_DBGMSG(%s):\n", tag);
	mutex_enter(&zfs_dbgmsgs.pl_lock);
	zfs_dbgmsg_t *zdm;
	for (zdm = list_head(&zfs_dbgmsgs.pl_list); zdm;
	    zdm = list_next(&zfs_dbgmsgs.pl_list, zdm))
		(void) printf("%s\n", zdm->zdm_msg);
	mutex_exit(&zfs_dbgmsgs.pl_lock);
}
#endif /* _KERNEL */

#ifdef _KERNEL
module_param(zfs_dbgmsg_enable, int, 0644);
MODULE_PARM_DESC(zfs_dbgmsg_enable, "Enable ZFS debug message log");

module_param(zfs_dbgmsg_maxsize, int, 0644);
MODULE_PARM_DESC(zfs_dbgmsg_maxsize, "Maximum ZFS debug log size");
#endif
