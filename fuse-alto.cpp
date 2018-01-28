/*******************************************************************************************
 *
 * Alto file system FUSE interface
 *
 * Copyright (c) 2016 Jürgen Buchmüller <pullmoll@t-online.de>
 *
 *******************************************************************************************/
#define FUSE_USE_VERSION 26

#include "config.h"
#include <fuse.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include "altofs.h"

// Prototypes
void printBufferChars(const char *buf, size_t size);
void printBuffer(const char *buf, size_t size);
void convertReadChars(char *buf, size_t size);
const char* convertWriteChars(const char *buf, size_t size);

// Globals
static struct fuse_args fuse_args;
static int verbose = 0;
static int help = 0;
static int debug = 0;
static int version = 0;
static int foreground = 0;
static int multithreaded = 1;
static int nonopt_seen = 0;
static char* mountpoint = NULL;
static char* filenames = NULL;
static struct fuse_chan* chan = NULL;
static struct fuse* fuse = NULL;
static struct fuse_operations* fuse_ops = NULL;
static AltoFS* afs = 0;

enum
{
	KEY_DEBUG,
	KEY_HELP,
	KEY_FOREGROUND,
	KEY_SINGLE_THREADED,
	KEY_VERBOSE,
	KEY_VERSION
};

/**
 * @brief List of options
 */
static struct fuse_opt alto_opts[] =
{
	FUSE_OPT_KEY("-d",           KEY_DEBUG),
	FUSE_OPT_KEY("--debug",      KEY_DEBUG),
	FUSE_OPT_KEY("-h",           KEY_HELP),
	FUSE_OPT_KEY("--help",       KEY_HELP),
	FUSE_OPT_KEY("-f",           KEY_FOREGROUND),
	FUSE_OPT_KEY("--foreground", KEY_FOREGROUND),
	FUSE_OPT_KEY("-s",           KEY_SINGLE_THREADED),
	FUSE_OPT_KEY("--single",     KEY_SINGLE_THREADED),
	FUSE_OPT_KEY("-v",           KEY_VERBOSE),
	FUSE_OPT_KEY("--verbose",    KEY_VERBOSE),
	FUSE_OPT_KEY("-V",           KEY_VERSION),
	FUSE_OPT_KEY("--version",    KEY_VERSION),
	FUSE_OPT_END
};

static int create_alto(const char* path, mode_t mode, dev_t dev)
{
	printf("\ncreate_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	
	printf("    ctx->pid:   0x%X\n", ctx->pid);
	printf("    ctx->uid:   0x%X\n", ctx->uid);
	printf("    ctx->gid:   0x%X\n", ctx->gid);
	printf("    ctx->umask: 0x%X\n", ctx->umask);
	printf("    ctx->fuse: 0x%lX\n", (uintptr_t)ctx->fuse);
	printf("    ctx->private_data: 0x%lX\n", (uintptr_t)ctx->private_data);

	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	int res = 0;
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (info)
	{
		res = afs->unlink_file(path);
		if (res < 0)
		{
			printf("create_alto: %s: unlink_file(\"%s\") returned %d\n", __func__, path, res);
			return res;
		}
	}
	
	res = afs->create_file(path);
	if (res < 0)
	{
		printf("create_alto: %s: create_file(\"%s\") returned %d\n", __func__, path, res);
		return res;
	}
	
	info = afs->find_fileinfo(path);
	// Something went really, really wrong
	if (!info)
	{
		printf("create_alto: %s  result: 0\n", path);
		return -ENOSPC;
	}
	
	printf("create_alto: %s  result: 0\n", path);

	return 0;
}

static int getattr_alto(const char *path, struct stat *stbuf)
{
	printf("\ngetattr_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	printf("    ctx->pid:   0x%X\n", ctx->pid);
	printf("    ctx->uid:   0x%X\n", ctx->uid);
	printf("    ctx->gid:   0x%X\n", ctx->gid);
	printf("    ctx->umask: 0x%X\n", ctx->umask);
	printf("    ctx->fuse: 0x%lX\n", (uintptr_t)ctx->fuse);
	printf("    ctx->private_data: 0x%lX\n", (uintptr_t)ctx->private_data);
	printf("\n");
	
	memset(stbuf, 0, sizeof(struct stat));

	afs_fileinfo* info = afs->find_fileinfo(path);
	
	printf("getattr_alto: %s  info: %p\n", path, info);

	if (!info)
	{
		printf("getattr_alto: %s  result: ENOENT\n", path);

		return -ENOENT;
	}
	
	page_t leaderPage = info->leader_page_vda();
	if (leaderPage != 0)
	{
		afs_leader_t* lp = afs->page_leader(info->leader_page_vda());
		afs->dump_leader(lp);
	}
	
	info->setStatUid(ctx->uid);
	info->setStatGid(ctx->gid);
	
	// Using the umask may cause an error!
	// Why the umask changes between calls?
	// printf("##### pid:0x%X umask:0x%X -> st_mode:0x%X\n", ctx->pid, ctx->umask, info->st()->st_mode & ~ctx->umask);
	info->setStatMode(info->statMode() /*& ~(ctx->umask)*/);
	
	memcpy(stbuf, info->st(), sizeof(*stbuf));
/*
	printf("    st_dev:     0x%X\n", info->st()->st_dev);
	printf("    st_ino:     0x%llX\n", info->st()->st_ino);
	printf("    st_mode:    0x%X\n", info->st()->st_mode);
	printf("    st_nlink:   0x%X\n", info->st()->st_nlink);
	printf("    st_uid:     0x%X\n", info->st()->st_uid);
	printf("    st_gid:     0x%X\n", info->st()->st_gid);
	printf("    st_rdev:    0x%X\n", info->st()->st_rdev);
	printf("    st_size:    0x%llX\n", info->st()->st_size);
	printf("    st_blocks:  0x%llX\n", info->st()->st_blocks);
	printf("    st_blksize: 0x%X\n", info->st()->st_blksize);
	printf("    st_flags:   0x%X\n", info->st()->st_flags);
	printf("    st_gen:     0x%X\n", info->st()->st_gen);
	printf("    st_lspare:	0x%X\n", info->st()->st_lspare);
	printf("    st_qspare:  0x%llX 0x%llX\n", info->st()->st_qspare[0], info->st()->st_qspare[1]);
*/
	printf("getattr_alto: %s  result: 0\n", path);

	return 0;
}

static int readdir_alto(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf("\nreaddir_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo("/");
	if (!info)
	{
		printf("readdir_alto: %s  result: ENOENT\n", path);

		return -ENOENT;
	}
	
	info->setStatUid(ctx->uid);
	info->setStatGid(ctx->gid);
	
	// Using the umask may cause an error!
	// Why the umask changes between calls?
	info->setStatMode(info->statMode() /*& ~ctx->umask*/);
	
	filler(buf, ".", info->st(), 0);
	filler(buf, "..", NULL, 0);
	
	printf("readdir_alto: parent: %p %s %d\n", info, info->name().c_str(), (int)info->size());
	
	for (int i = 0; i < info->size(); i++)
	{
		afs_fileinfo* child = info->child(i);
		if (!child)
		{
			printf("    NULL CHILD!!!\n");
			continue;
		}
		
		printf("    %s", child->name().c_str());
		
		if (child->deleted())
		{
			printf(" DELETED!\n");
			continue;
		}
		
		child->setStatUid(ctx->uid);
		child->setStatGid(ctx->gid);
		
		// Using the umask may cause an error!
		// Why the umask changes between calls?
		child->setStatMode(child->statMode() /* & ~ctx->umask*/);
		
		if (filler(buf, child->name().c_str(), child->st(), 0))
		{
			break;
		}
		
		printf("\n");
	}
	
	printf("readdir_alto: %s  result: 0\n", path);

	return 0;
}

static int open_alto(const char *path, struct fuse_file_info *fi)
{
	printf("\nopen_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		printf("open_alto: %s  result: ENOENT\n", path);

		return -ENOENT;
	}
	
	fi->fh = (uint64_t)info;
	
	printf("open_alto: %s  result: 0\n", path);

	return 0;
}

static int read_alto(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info*)
{
	printf("\nread_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		printf("read_alto: %s  result: ENOENT\n", path);

		return -ENOENT;
	}

	printf("read_alto: %s  st_size:%lld\n", path, info->st()->st_size);

	if (offset >= info->st()->st_size)
	{
		printf("read_alto: %s  result: 0\n", path);

		return 0;
	}

	printf("read_alto: %s  vda:0x%zX  size:%zu buf:%p offset:%lld\n", path, info->leader_page_vda(), size, buf, offset);

	size_t done = afs->read_file(info->leader_page_vda(), buf, size, offset);
	
	printf("read_alto: %s  vda:0x%zX size:%zu buf:%p offset:%lld  result: %zu\n", path, info->leader_page_vda(), size, buf, offset, done);
	
	// printBufferChars(buf, size);
	// printBuffer(buf, size);
	
	// Convert some chars from Alto to Mac
	convertReadChars(buf, size);
	
	printf("read_alto: %s  result: %zu\n", path, done);

	return (int)done;
}

static int write_alto(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info*)
{
	printf("\nwrite_alto: %s\n", path);
	
	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		printf("write_alto: %s  result: ENOENT\n", path);

		return -ENOENT;
	}
	
	printf("write_alto: %s  st_size:%lld\n", path, info->st()->st_size);

	// Convert some chars from Mac to Alto
	const char *convBuff = convertWriteChars(buf, size);
	
	size_t done = afs->write_file(info->leader_page_vda(), convBuff, size, offset);
	
	// free the newly allocated buffer if there is one
	if (convBuff != buf)
	{
		free((void*)convBuff);
	}
	
	printf("write_alto: size: %zu  offset: %lld  result: %zu\n", size, offset, done);

	info = afs->find_fileinfo(path);
	printf("write_alto: %s  st_size:%lld\n", path, info->st()->st_size);

	return (int)done;
}

static int truncate_alto(const char* path, off_t offset)
{
	printf("\ntruncate_alto: %s  offset:%lld\n", path, offset);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	printf("#### truncate_alto: %s  st_size:%lld\n", path, info->st()->st_size);
	
	int result = 0;
	if(offset != info->st()->st_size)
	{
		result = afs->truncate_file(path, offset);

		info = afs->find_fileinfo(path);
		printf("#### truncate_alto: %s  st_size:%lld result: %d\n", path, info->st()->st_size, result);
	}
	
	return result;
}

static int unlink_alto(const char *path)
{
	printf("\nunlink_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	int result = afs->unlink_file(path);
	
	printf("unlink_alto: %s  result: %d\n", path, result);

	return result;
}

static int rename_alto(const char *path, const char* newname)
{
	printf("\nrename_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	int result = afs->rename_file(path, newname);
	
	printf("rename_alto: %s  result: %d\n", path, result);
	
	return result;
}

static int utimens_alto(const char* path, const struct timespec tv[2])
{
	printf("\nutimens_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	int result = afs->set_times(path, tv);
	
	printf("utimens_alto: %s  result: %d\n", path, result);
	
	return result;
}

static int statfs_alto(const char *path, struct statvfs* vfs)
{
	printf("\nstatfs_alto: %s\n", path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	// We have but a single root directory
	if (strcmp(path, "/"))
	{
		printf("statfs_alto: %s  result: EINVAL\n", path);

		return -EINVAL;
	}
	
	int result = afs->statvfs(vfs);

	printf("statfs_alto: %s  result: %d\n", path, result);

	return result;
}

#if DEBUG
static const char* fuse_cap(unsigned flags)
{
	static char buff[512];
	char* dst = buff;
	memset(buff, 0, sizeof(buff));
	
	if (flags & FUSE_CAP_ASYNC_READ)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", ASYNC_READ");
	if (flags & FUSE_CAP_POSIX_LOCKS)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", POSIX_LOCKS");
	if (flags & FUSE_CAP_ATOMIC_O_TRUNC)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", ATOMIC_O_TRUNC");
	if (flags & FUSE_CAP_EXPORT_SUPPORT)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", EXPORT_SUPPORT");
	if (flags & FUSE_CAP_BIG_WRITES)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", BIG_WRITES");
	if (flags & FUSE_CAP_DONT_MASK)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", DONT_MASK");
	if (flags & FUSE_CAP_SPLICE_WRITE)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", SPLICE_WRITE");
	if (flags & FUSE_CAP_SPLICE_MOVE)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", SPLICE_MOVE");
	if (flags & FUSE_CAP_SPLICE_READ)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", SPLICE_READ");
	if (flags & FUSE_CAP_FLOCK_LOCKS)
		dst += snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", FLOCK_LOCKS");
	if (flags & FUSE_CAP_IOCTL_DIR)
		snprintf(dst, sizeof(buff) - (size_t)(dst - buff), "%s", ", IOCTL_DIR");
	return buff + 2;
}
#endif

void* init_alto(fuse_conn_info* info)
{
	(void)info;
	
	afs = new AltoFS(filenames, verbose);
	
#if DEBUG
	if (verbose > 2)
	{
		printf("%s: fuse_conn_info* = %p\n", __func__, (void*)info);
		printf("%s:   proto_major             : %u\n", __func__, info->proto_major);
		printf("%s:   proto_minor             : %u\n", __func__, info->proto_minor);
		printf("%s:   async_read              : %u\n", __func__, info->async_read);
		printf("%s:   max_write               : %u\n", __func__, info->max_write);
		printf("%s:   max_readahead           : %u\n", __func__, info->max_readahead);
		printf("%s:   capable                 : %#x\n", __func__, info->capable);
		printf("%s:   capable.flags           : %s\n", __func__, fuse_cap(info->capable));
		printf("%s:   want                    : %#x\n", __func__, info->want);
		printf("%s:   want.flags              : %s\n", __func__, fuse_cap(info->want));
		printf("%s:   max_background          : %u\n", __func__, info->max_background);
		printf("%s:   congestion_threshold    : %u\n", __func__, info->congestion_threshold);
	}
#endif
	return afs;
}

static int usage(const char* program)
{
	const char* prog = strrchr(program, '/');
	prog = prog ? prog + 1 : program;
	
	fprintf(stderr, "fuse-alto Version %s by Luca Severini (lucaseverini@mac.com)\n", FUSE_ALTO_VERSION);
	fprintf(stderr, "usage: %s <mountpoint> [options] <disk image file(s)>\n", prog);
	fprintf(stderr, "Where [options] can be one or more of\n");
	fprintf(stderr, "    -h|--help          print this help\n");
	fprintf(stderr, "    -f|--foreground    run fuse-alto in the foreground\n");
	fprintf(stderr, "    -s|--single        run fuse-alto single threaded\n");
	fprintf(stderr, "    -v|--verbose       set verbose mode (can be repeated)\n");
	return 0;
}

static int is_alto_opt(const char* arg)
{
	// no "-o option" yet
	return 0;
}

static int alto_fuse_main(struct fuse_args *args)
{
	return fuse_main(args->argc, args->argv, fuse_ops, NULL);
}

static int fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs)
{
	// char* arg0 = reinterpret_cast<char *>(data);
	
	switch (key)
	{
		case FUSE_OPT_KEY_OPT:
			if (is_alto_opt(arg))
			{
				size_t size = strlen(arg) + 3;
				char* tmp = new char[size];
				snprintf(tmp, size, "-o%s", arg);
				// Not yet
				// fuse_opt_add_arg(atlto_args, arg, -1);
				delete[] tmp;
				
				return 0;
			}
			break;
			
		case FUSE_OPT_KEY_NONOPT:
			if (0 == nonopt_seen++)
			{
				return 1;
			}
			
			if (NULL == filenames)
			{
				size_t size = strlen(arg) + 1;
				filenames = new char[size];
				snprintf(filenames, size, "%s", arg);
				
				return 0;
			}
			else
			{
				size_t size = strlen(filenames) + 1 + strlen(arg) + 1;
				char* tmp = new char[size];
				snprintf(tmp, size, "%s,%s", filenames, arg);
				delete[] filenames;
				filenames = tmp;
				
				return 0;
			}
			break;
			
		case KEY_HELP:
			help = 1;
			return 0;

		case KEY_DEBUG:
			debug = 1;
			return 0;

		case KEY_FOREGROUND:
			foreground = 1;
			return 0;
			
		case KEY_SINGLE_THREADED:
			multithreaded = 0;
			return 1;
			
		case KEY_VERBOSE:
			verbose++;
			return 0;
			
		case KEY_VERSION:
			version = 1;
			return 0;
			
		default:
			fprintf(stderr, "internal error\n");
			exit(2);
	}
	
	return 0;
}

static void shutdown_fuse()
{
	delete afs;
	afs = nullptr;
	
	if (fuse)
	{
		if (verbose)
		{
			printf("%s: removing signal handlers\n", __func__);
		}
		
		fuse_remove_signal_handlers(fuse_get_session(fuse));
	}
	
	if (mountpoint && chan)
	{
		if (verbose)
		{
			printf("%s: unmounting %s\n", __func__, mountpoint);
		}
		
		fuse_unmount(mountpoint, chan);
		mountpoint = 0;
		chan = 0;
	}
	
	if (fuse)
	{
		if (verbose)
		{
			printf("%s: shutting down fuse\n", __func__);
		}
		
		fuse_destroy(fuse);
		fuse = 0;
	}
	
	if (fuse_ops)
	{
		if (verbose)
		{
			printf("%s: releasing fuse ops\n", __func__);
		}
		
		free(fuse_ops);
		fuse_ops = 0;
	}
	
	if(fuse_args.allocated != 0)
	{
		if (verbose)
		{
			printf("%s: releasing fuse args\n", __func__);
		}
		
		fuse_opt_free_args(&fuse_args);
	}
}

int main(int argc, char *argv[])
{
#if DEBUG
	printf("%s pid:%d\n", basename(argv[0]), getpid());
#endif

	if (argc == 1)
	{
		usage(argv[0]);
		
		exit(1);
	}

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	memcpy(&fuse_args, &args, sizeof(fuse_args));
	
	int res;
	res = fuse_opt_parse(&fuse_args, argv[0], alto_opts, fuse_opt_proc);
	if (res == -1)
	{
		perror("fuse_opt_parse()");
		exit(1);
	}
	
	if (verbose)
	{
#if RELEASE
		printf("%s pid:%d\n", basename(argv[0]), getpid());
#endif
	}

	if (debug)
	{
		foreground = 1;
		
		fuse_opt_add_arg(&fuse_args, "-d"); // "-d" or "-odebug"
	}

	if (foreground)
	{
		fuse_opt_add_arg(&fuse_args, "-f");
	}
	
	if (version)
	{
		if (debug == 0 && help == 0)
		{
			printf("fuse-alto Version %s by Luca Severini (lucaseverini@mac.com)\n", FUSE_ALTO_VERSION);

			// Current fuse version always prints the version
			fuse_opt_add_arg(&fuse_args, "--version");
			
			alto_fuse_main(&fuse_args);
			
			// Fuse can't run with option --version
			exit(0);
		}
	}

	if (help)
	{
		usage(argv[0]);
		
		fuse_opt_add_arg(&fuse_args, "--help");
		
		alto_fuse_main(&fuse_args);
		
		exit(0);
	}
	
	res = fuse_parse_cmdline(&fuse_args, &mountpoint, &multithreaded, &foreground);
	if (res == -1)
	{
		perror("fuse_parse_cmdline()");
		exit(1);
	}
	
	assert(sizeof(afs_kdh_t) == 32);
	assert(sizeof(afs_leader_t) == PAGESZ);
	
	fuse_ops = reinterpret_cast<struct fuse_operations *>(calloc(1, sizeof(*fuse_ops)));
	fuse_ops->getattr = getattr_alto;
	fuse_ops->unlink = unlink_alto;
	fuse_ops->rename = rename_alto;
	fuse_ops->open = open_alto;
	fuse_ops->read = read_alto;
	fuse_ops->write = write_alto;
	fuse_ops->mknod = create_alto;
	fuse_ops->truncate = truncate_alto;
	fuse_ops->readdir = readdir_alto;
	fuse_ops->utimens = utimens_alto;
	fuse_ops->statfs = statfs_alto;
	fuse_ops->init = init_alto;
	
	atexit(shutdown_fuse);
	
	struct stat st;
	res = stat(mountpoint, &st);
	if (res == -1)
	{
		perror(mountpoint);
		exit(1);
	}
	
	if (!S_ISDIR(st.st_mode))
	{
		perror(mountpoint);
		exit(1);
	}
	
	chan = fuse_mount(mountpoint, &fuse_args);
	if (0 == chan)
	{
		perror("fuse_mount()");
		exit(1);
	}
	
	fuse = fuse_new(chan, &fuse_args, fuse_ops, sizeof(*fuse_ops), NULL);
	if (0 == fuse)
	{
		perror("fuse_new()");
		exit(2);
	}
	
	res = fuse_daemonize(foreground);
	if (res != -1)
	{
		res = fuse_set_signal_handlers(fuse_get_session(fuse));
	}
	
	if (res != -1)
	{
		if (multithreaded)
		{
			res = fuse_loop_mt(fuse);
		}
		else
		{
			res = fuse_loop(fuse);
		}
	}
	
	shutdown_fuse();

#if DEBUG
	printf("%s quits now.\n", basename(argv[0]));
#else
	if(verbose)
	{
		printf("%s quits now.\n", basename(argv[0]));
	}
#endif
	return res;
}

void printBuffer(const char *buf, size_t size)
{
	if (size == 0)
	{
		return;
	}
	
	printf("#### %d chars in buffer:\n", (int)size);
	
	char prevChar = '\0';
	for (int idx = 0; idx < size; idx++)
	{
		switch (buf[idx])
		{
			case '\f':
				printf("\\f");
				break;
				
			case '\b':
				printf("\\b");
				break;
				
			case '\v':
				printf("\\v");
				break;
				
			case '\t':
				printf("    ");
				break;
				
			case '\r':
			case '\n':
				printf("\n");
				break;
				
			default:
				printf("%c", (char)buf[idx]);
		}
		
		prevChar = buf[idx];
	}
	
	printf("#############################\n");
}

void printBufferChars(const char *buf, size_t size)
{
	printf("\n#### %d chars in buffer:\n", (int)size);

	if (size == 0)
	{
		return;
	}
	
	for (int idx = 0; idx < size; idx++)
	{
		printf("  [%d] %02hhX ", idx, buf[idx]);
		
		switch (buf[idx])
		{
			case ' ':
				printf("\' \'\n");
				break;

			case '\f':
				printf("\\f\n");
				break;

			case '\b':
				printf("\\b\n");
				break;

			case '\v':
				printf("\\v\n");
				break;

			case '\t':
				printf("\\t\n");
				break;

			case '\r':
				printf("\\r\n");
				break;

			case '\n':
				printf("\\n\n");
				break;
				
			default:
				printf("%c\n", (char)buf[idx]);
		}
	}
	
	printf("#############################\n");
}

void convertReadChars(char *buf, size_t size)
{
	for (int idx = 0; idx < size; idx++)
	{
		switch (buf[idx])
		{
			case '\r':
				buf[idx] = '\n';
				break;
		}
	}
}

const char* convertWriteChars(const char *buf, size_t size)
{
	char *convertedBuf = NULL;
	
	for (int idx = 0; idx < size; idx++)
	{
		switch (buf[idx])
		{
			case '\n':
				if(convertedBuf == NULL)
				{
					convertedBuf = (char*)malloc(size);
					assert(convertedBuf != NULL);
					memcpy(convertedBuf, buf, size);
					buf = convertedBuf;
				}
				
				convertedBuf[idx] = '\r';
				break;
		}
	}
	
	return buf;
}

