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
 * Copyright (c) 2018 by Delphix. All rights reserved.
 */

#ifndef	_SYS_ZFS_DEBUG_IMPL_H
#define	_SYS_ZFS_DEBUG_IMPL_H

#include <sys/procfs_list.h>

typedef struct zfs_dbgmsg {
	procfs_list_node_t	zdm_node;
	time_t			zdm_timestamp;
	int			zdm_size;
	char			zdm_msg[1]; /* variable length allocation */
} zfs_dbgmsg_t;

#endif	/* _SYS_ZFS_DEBUG_IMPL_H */
