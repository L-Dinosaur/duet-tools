/*
 * Copyright (C) 2014-2015 George Amvrosiadis.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "commands.h"

#if 0
static const char * const task_cmd_group_usage[] = {
	"duet task <command> [options]",
	NULL
};

static const char * const cmd_task_fetch_usage[] = {
	"duet task fetch [-i taskid] [-n num]",
	"Fetched up to num items for task with ID taskid, and prints them.",
	"",
	"-i	task ID used to find the task",
	"-n	number of events (default: 512, max: 65535)",
	NULL
};

static const char * const cmd_task_reg_usage[] = {
	"duet task register [-n name] [-b bitrange] [-m nmodel] [-p path]",
	"Registers a new task with the currently active framework. The task",
	"will be assigned an ID, and will be registered under the provided",
	"name. The bitmaps that keep information on what has been processed",
	"can be customized to store a given range of numbers per bit.",
	"",
	"-n     name under which to register the task",
	"-b     range of items/bytes per bitmap bit",
	"-m     event mask for task",
	"-p     path of the root of the namespace of interest",
	NULL
};

static const char * const cmd_task_dereg_usage[] = {
	"duet task deregister [-i taskid]",
	"Deregisters an existing task from the currently active framework.",
	"The task is tracked using the given ID. This command is mainly used",
	"for debugging purposes.",
	"",
	"-i     task ID used to find the task",
	NULL
};

static const char * const cmd_task_mark_usage[] = {
	"duet task mark [-i id] [-o offset] [-l len]",
	"Marks a block range for a specific task.",
	"Finds and marks the given block range (in bytes), in the bitmaps",
	"of the task with the given id.",
	"",
	"-i     the id of the task",
	"-o     the offset denoting the beginning of the range in bytes",
	"-l     the number of bytes denoting the length of the range",
	NULL
};

static const char * const cmd_task_unmark_usage[] = {
	"duet task unmark [-i id] [-o offset] [-l len]",
	"Unmarks a block range for a specific task.",
	"Finds and unmarks the given block range (in bytes), in the bitmaps",
	"of the task wit the given id.",
	"",
	"-i     the id of the task",
	"-o     the offset denoting the beginning of the range in bytes",
	"-l     the number of bytes denoting the length of the range",
	NULL
};

static const char * const cmd_task_check_usage[] = {
	"duet task check [-i id] [-o offset] [-l len]",
	"Checks if a block range for a specific task is marked or not.",
	"Finds and checks if the given block range (in bytes) is marked or not",
	"in the bitmaps of the task wit the given id.",
	"",
	"-i     the id of the task",
	"-o     the offset denoting the beginning of the range in bytes",
	"-l     the number of bytes denoting the length of the range",
	NULL
};

static int cmd_task_fetch(int fd, int argc, char **argv)
{
	int c, count = 512, tid = 0, ret = 0;
	struct duet_item *items = NULL;

	optind = 1;
	while ((c = getopt(argc, argv, "i:n:")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			tid = (int)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid ID");
				usage(cmd_task_fetch_usage);
			}
			break;
		case 'n':
			errno = 0;
			count = (int)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid event count");
				usage(cmd_task_fetch_usage);
			}
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_fetch_usage);
		}
	}

	if (!tid || !count || argc != optind)
		usage(cmd_task_fetch_usage);

	/* Allocate event buffer before passing to fetch */
	items = malloc(count * sizeof(struct duet_item));
	if (!items) {
		perror("fetch: buffer allocation error");
		usage(cmd_task_fetch_usage);
	}

	ret = duet_fetch(fd, tid, items, &count);
	if (ret < 0) {
		perror("fetch: ioctl error");
		free(items);
		usage(cmd_task_fetch_usage);
	}

	if (count == 0) {
		fprintf(stdout, "Received no items.\n");
		goto out;
	}

	/* Print out the list we received */
	fprintf(stdout, "UUID            \tInode number\tGeneration\tOffset      \tState   \n"
			"----------------\t------------\t----------\t------------\t--------\n");
	for (c=0; c<count; c++) {
		fprintf(stdout, "%16llx\t%12lu\t%10lu\t%12lu\t%8x\n",
			items[c].uuid, DUET_UUID_INO(items[c].uuid),
			DUET_UUID_GEN(items[c].uuid), items[c].idx << 12,
			items[c].state);
	}

out:
	free(items);
	return ret;
}

static int cmd_task_reg(int fd, int argc, char **argv)
{
	int c, tid, len=0, ret=0;
	char path[DUET_MAX_PATH], name[DUET_MAX_NAME];
	__u32 regmask = 0;
	__u32 bitrange = 0;

	path[0] = name[0] = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "n:b:m:p:")) != -1) {
		switch (c) {
		case 'n':
			len = strnlen(optarg, DUET_MAX_NAME);
			if (len == DUET_MAX_NAME || !len) {
				fprintf(stderr, "Invalid name (%d)\n", len);
				usage(cmd_task_reg_usage);
			}

			memcpy(name, optarg, DUET_MAX_NAME);
			break;
		case 'b':
			errno = 0;
			bitrange = (__u32)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtoll: invalid block size");
				usage(cmd_task_reg_usage);
			}
			break;
		case 'm':
			errno = 0;
			regmask = (__u32)strtol(optarg, NULL, 16);
			if (errno) {
				perror("strtol: invalid evtmask");
				usage(cmd_task_reg_usage);
			}
			break;
		case 'p':
			errno = 0;
			memcpy(path, optarg, DUET_MAX_PATH);
			if (errno)
				perror("memcpy: invalid path");
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_reg_usage);
		}
	}

	if (!name[0] || argc != optind)
		usage(cmd_task_reg_usage);

	ret = duet_register(fd, path, regmask, bitrange, name, &tid);
	if (ret) {
		fprintf(stdout, "Error registering task '%s'\n", name);
		usage(cmd_task_reg_usage);
	}

	fprintf(stdout, "Success registering task '%s' (ID %d)\n", name, tid);
	return ret;
}

static int cmd_task_dereg(int fd, int argc, char **argv)
{
	int c, tid = 0, ret = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "i:")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			tid = (int)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid ID");
				usage(cmd_task_dereg_usage);
			}
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_dereg_usage);
		}
	}

	if (!tid || argc != optind)
		usage(cmd_task_dereg_usage);

	ret = duet_deregister(fd, tid);
	if (ret) {
		fprintf(stdout, "Error deregistering task (ID %d)\n", tid);
		usage(cmd_task_dereg_usage);
	}

	fprintf(stdout, "Success deregistering task (ID %d)\n", tid);
	return ret;
}

static int cmd_task_mark(int fd, int argc, char **argv)
{
	int c, tid = 0, ret = 0;
	__u64 idx = 0;
	__u32 count = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "i:o:l:")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			tid = (__u8)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid ID");
				usage(cmd_task_mark_usage);
			}
			break;
		case 'o':
			errno = 0;
			idx = (__u64)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtoll: invalid offset");
				usage(cmd_task_mark_usage);
			}
			break;
		case 'l':
			errno = 0;
			count = (__u32)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid length");
				usage(cmd_task_mark_usage);
			}
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_mark_usage);
		}
	}

	if (!tid || !count || argc != optind)
		usage(cmd_task_mark_usage);

	ret = duet_set_done(fd, tid, idx, count);
	if (ret < 0) {
		perror("debug addblk ioctl error");
		usage(cmd_task_mark_usage);
	}

	fprintf(stdout, "Success adding blocks [%llu, %llu] to task #%d.\n",
		idx, idx + count, tid);
	return ret;
}

static int cmd_task_unmark(int fd, int argc, char **argv)
{
	int c, tid = 0, ret = 0;
	__u64 idx = 0;
	__u32 count = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "i:o:l:")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			tid = (__u8)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid ID");
				usage(cmd_task_unmark_usage);
			}
			break;
		case 'o':
			errno = 0;
			idx = (__u64)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtoll: invalid offset");
				usage(cmd_task_unmark_usage);
			}
			break;
		case 'l':
			errno = 0;
			count = (__u32)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid length");
				usage(cmd_task_unmark_usage);
			}
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_unmark_usage);
		}
	}

	if (!tid || !count || argc != optind)
		usage(cmd_task_unmark_usage);

	ret = duet_unset_done(fd, tid, idx, count);
	if (ret < 0) {
		perror("debug rmblk ioctl error");
		usage(cmd_task_unmark_usage);
	}

	fprintf(stdout, "Success removing blocks [%llu, %llu] to task #%d.\n",
		idx, idx + count, tid);
	return ret;
}

static int cmd_task_check(int fd, int argc, char **argv)
{
	int c, tid = 0, ret = 0;
	__u64 idx = 0;
	__u32 count = 0;

	optind = 1;
	while ((c = getopt(argc, argv, "i:o:l:")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			tid = (__u8)strtol(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid ID");
				usage(cmd_task_check_usage);
			}
			break;
		case 'o':
			errno = 0;
			idx = (__u64)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtoll: invalid offset");
				usage(cmd_task_check_usage);
			}
			break;
		case 'l':
			errno = 0;
			count = (__u32)strtoll(optarg, NULL, 10);
			if (errno) {
				perror("strtol: invalid length");
				usage(cmd_task_check_usage);
			}
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", (char)c);
			usage(cmd_task_check_usage);
		}
	}

	if (!tid || !count || argc != optind)
		usage(cmd_task_check_usage);

	ret = duet_check_done(fd, tid, idx, count);
	if (ret < 0) {
		perror("debug chkblk ioctl error");
		usage(cmd_task_check_usage);
	}

	fprintf(stdout, "Blocks [%llu, %llu] in task #%d were %sset.\n",
		idx, idx + count, tid, ret ? "" : "not ");
	return 0;
}

const struct cmd_group task_cmd_group = {
	task_cmd_group_usage, NULL, {
		{ "register", cmd_task_reg, cmd_task_reg_usage, NULL, 0 },
		{ "deregister", cmd_task_dereg, cmd_task_dereg_usage, NULL, 0 },
		{ "mark", cmd_task_mark, cmd_task_mark_usage, NULL, 0 },
		{ "unmark", cmd_task_unmark, cmd_task_unmark_usage, NULL, 0 },
		{ "check", cmd_task_check, cmd_task_check_usage, NULL, 0 },
		{ "fetch", cmd_task_fetch, cmd_task_fetch_usage, NULL, 0 },
	}
};

int cmd_task(int fd, int argc, char **argv)
{
	return handle_command_group(&task_cmd_group, fd, argc, argv);
}
#endif /* 0 */
