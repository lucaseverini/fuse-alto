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
static int verbose = 0;
static struct fuse_args fuse_args;
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
static bool check = false;
static bool rebuild = false;

enum
{
	KEY_DEBUG,
	KEY_HELP,
	KEY_FOREGROUND,
	KEY_SINGLE_THREADED,
	KEY_VERBOSE,
	KEY_VERSION,
	KEY_CHECK,
	KEY_REBUILD
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
	FUSE_OPT_KEY("--c",    		 KEY_CHECK),
	FUSE_OPT_KEY("--check",    	 KEY_CHECK),
	FUSE_OPT_KEY("--r",    		 KEY_REBUILD),
	FUSE_OPT_KEY("--rebuild",    KEY_REBUILD),
	FUSE_OPT_END
};

void log(int verbosity, const char* format, ...)
{
	if (verbosity > verbose)
	{
		return;
	}
	
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	
	fflush(stdout);
}

static int create_alto(const char* path, mode_t mode, dev_t dev)
{
	log(2, "%s: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	
	log(3, "%s: ctx->pid:   0x%X\n", __func__, ctx->pid);
	log(3, "%s: ctx->uid:   0x%X\n", __func__, ctx->uid);
	log(3, "%s: ctx->gid:   0x%X\n", __func__, ctx->gid);
	log(3, "%s: ctx->umask: 0x%X\n", __func__, ctx->umask);
	log(3, "%s: ctx->fuse: 0x%lX\n", __func__, (uintptr_t)ctx->fuse);
	log(3, "%s: ctx->private_data: 0x%lX\n", __func__, (uintptr_t)ctx->private_data);

	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	int res = 0;
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (info)
	{
		res = afs->unlink_file(path);
		if (res < 0)
		{
			log(1, "%s: unlink_file(\"%s\") returned %d\n", __func__, path, res);
			return res;
		}
	}
	
	res = afs->create_file(path);
	if (res < 0)
	{
		log(1, "%s: create_file(\"%s\") returned %d\n", __func__, path, res);
		return res;
	}
	
	info = afs->find_fileinfo(path);
	// Something went really, really wrong
	if (!info)
	{
		log(1, "%s: result: 0\n", __func__, path);
		return -ENOSPC;
	}
	
	log(2, "%s: result: 0\n", __func__, path);

	return 0;
}

static int getattr_alto(const char *path, struct stat *stbuf)
{
	log(2, "%s: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	log(3, "%s: ctx->pid:   0x%X\n", __func__, ctx->pid);
	log(3, "%s: ctx->uid:   0x%X\n", __func__, ctx->uid);
	log(3, "%s: ctx->gid:   0x%X\n", __func__, ctx->gid);
	log(3, "%s: ctx->umask: 0x%X\n", __func__, ctx->umask);
	log(3, "%s: ctx->fuse: 0x%lX\n", __func__, (uintptr_t)ctx->fuse);
	log(3, "%s: ctx->private_data: 0x%lX\n", __func__, (uintptr_t)ctx->private_data);

	memset(stbuf, 0, sizeof(struct stat));

	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		log(2, "%s: %s result: ENOENT\n", __func__, path);

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

	log(3, "    st_dev:     0x%X\n", info->st()->st_dev);
	log(3, "    st_ino:     0x%llX\n", info->st()->st_ino);
	log(3, "    st_mode:    0x%X\n", info->st()->st_mode);
	log(3, "    st_nlink:   0x%X\n", info->st()->st_nlink);
	log(3, "    st_uid:     0x%X\n", info->st()->st_uid);
	log(3, "    st_gid:     0x%X\n", info->st()->st_gid);
	log(3, "    st_rdev:    0x%X\n", info->st()->st_rdev);
	log(3, "    st_size:    0x%llX\n", info->st()->st_size);
	log(3, "    st_blocks:  0x%llX\n", info->st()->st_blocks);
	log(3, "    st_blksize: 0x%X\n", info->st()->st_blksize);
	log(3, "    st_flags:   0x%X\n", info->st()->st_flags);
	log(3, "    st_gen:     0x%X\n", info->st()->st_gen);
	log(3, "    st_lspare:	0x%X\n", info->st()->st_lspare);
	log(3, "    st_qspare:  0x%llX 0x%llX\n", info->st()->st_qspare[0], info->st()->st_qspare[1]);

	log(2, "%s: path: %s result: 0\n", __func__, path);

	return 0;
}

static int readdir_alto(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo("/");
	if (!info)
	{
		log(2, "%s: path: %s result: ENOENT\n", __func__, path);

		return -ENOENT;
	}
	
	info->setStatUid(ctx->uid);
	info->setStatGid(ctx->gid);
	
	// Using the umask may cause an error!
	// Why the umask changes between calls?
	info->setStatMode(info->statMode() /*& ~ctx->umask*/);
	
	filler(buf, ".", info->st(), 0);
	filler(buf, "..", NULL, 0);
	
	log(2, "%s: parent: %p %s %d\n", __func__, info, info->name().c_str(), (int)info->size());
	
	for (int i = 0; i < info->size(); i++)
	{
		afs_fileinfo* child = info->child(i);
		if (!child)
		{
			printf(" NULL CHILD!!!\n");
			continue;
		}
		
		if (child->deleted())
		{
			// printf(" DELETED!\n");
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
	}
	
	log(2, "%s: path: %s result: 0\n", __func__, path);

	return 0;
}

static int open_alto(const char *path, struct fuse_file_info *fi)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		log(1, "%s: path: %s result: ENOENT\n", __func__, path);

		return -ENOENT;
	}
	
	fi->fh = (uint64_t)info;
	
	log(2, "%s: path: %s  result: 0\n", __func__, path);

	return 0;
}

static int read_alto(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info*)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		log(1, "%s: path: %s result: ENOENT\n", __func__, path);

		return -ENOENT;
	}

	log(2, "%s: path: %s st_size:%lld\n", __func__, path, info->st()->st_size);

	if (offset >= info->st()->st_size)
	{
		log(1, "%s: path: %s result: 0\n", __func__, path);

		return 0;
	}

	log(2, "%s: path: %s vda:0x%zX  size:%zu buf:%p offset:%lld\n", __func__, path, info->leader_page_vda(), size, buf, offset);

	size_t done = afs->read_file(info->leader_page_vda(), buf, size, offset);
	
	log(2, "%s: path: %s vda:0x%zX size:%zu buf:%p offset:%lld  result: %zu\n", __func__, path, info->leader_page_vda(), size, buf, offset, done);
	
	// printBufferChars(buf, size);
	// printBuffer(buf, size);
	
	// Convert some chars from Alto to Mac
	convertReadChars(buf, size);
	
	log(2, "%s: path: %s result: %zu\n", __func__, path, done);

	return (int)done;
}

static int write_alto(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info*)
{
	log(2, "%s: path: %s\n", __func__, path);
	
	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	if (!info)
	{
		log(1, "%s: path: %s result: ENOENT\n", __func__, path);

		return -ENOENT;
	}
	
	log(2, "%s: path: %s st_size:%lld\n", __func__, path, info->st()->st_size);

	afs->print_file_pages(info->leader_page_vda());

	// Convert some chars from Mac to Alto
	const char *convBuff = convertWriteChars(buf, size);
	
	size_t done = afs->write_file(info->leader_page_vda(), convBuff, size, offset);
	
	// free the newly allocated buffer if there is one
	if (convBuff != buf)
	{
		free((void*)convBuff);
	}
	
	log(2, "%s: path: size: %zu  offset: %lld  result: %zu\n", __func__, size, offset, done);

	info = afs->find_fileinfo(path);
	log(2, "%s: path: %s st_size:%lld\n", __func__, path, info->st()->st_size);

	afs->print_file_pages(info->leader_page_vda());
	
	return (int)done;
}

static int truncate_alto(const char* path, off_t offset)
{
	log(2, "%s: path: %s offset:%lld\n", __func__, path, offset);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	afs_fileinfo* info = afs->find_fileinfo(path);
	log(2, "%s: st_size:%lld\n", __func__, info->st()->st_size);
	
	afs->print_file_pages(info->leader_page_vda());

	int result = 0;
	if(offset != info->st()->st_size)
	{
		result = afs->truncate_file(path, offset);

		info = afs->find_fileinfo(path);
	}

	log(2, "%s: st_size:%lld result: %d\n", __func__, info->st()->st_size, result);

	afs->print_file_pages(info->leader_page_vda());

	return result;
}

static int unlink_alto(const char *path)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	int result = afs->unlink_file(path);
	
	log(2, "%s: path: %s result: %d\n", __func__, path, result);

	return result;
}

static int rename_alto(const char *path, const char* newname)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	int result = afs->rename_file(path, newname);
	
	log(2, "%s: path: %s result: %d\n", __func__, path, result);
	
	return result;
}

static int utimens_alto(const char* path, const struct timespec tv[2])
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);

	int result = afs->set_times(path, tv);
	
	log(2, "%s: path: %s result: %d\n", __func__, path, result);
	
	return result;
}

static int statfs_alto(const char *path, struct statvfs* vfs)
{
	log(2, "%s: path: %s\n", __func__, path);

	struct fuse_context* ctx = fuse_get_context();
	AltoFS* afs = reinterpret_cast<AltoFS*>(ctx->private_data);
	
	// We have but a single root directory
	if (strcmp(path, "/"))
	{
		log(2, "%s: path: %s  result: EINVAL\n", __func__, path);

		return -EINVAL;
	}
	
	int result = afs->statvfs(vfs);

	log(2, "%s: path: %s  result: %d\n", __func__, path, result);

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
	
	afs = new AltoFS(filenames, verbose, check, rebuild);
	
#if DEBUG
	log(3, "%s: fuse_conn_info* = %p\n", __func__, (void*)info);
	log(3, "%s:   proto_major             : %u\n", __func__, info->proto_major);
	log(3, "%s:   proto_minor             : %u\n", __func__, info->proto_minor);
	log(3, "%s:   async_read              : %u\n", __func__, info->async_read);
	log(3, "%s:   max_write               : %u\n", __func__, info->max_write);
	log(3, "%s:   max_readahead           : %u\n", __func__, info->max_readahead);
	log(3, "%s:   capable                 : %#x\n", __func__, info->capable);
	log(3, "%s:   capable.flags           : %s\n", __func__, fuse_cap(info->capable));
	log(3, "%s:   want                    : %#x\n", __func__, info->want);
	log(3, "%s:   want.flags              : %s\n", __func__, fuse_cap(info->want));
	log(3, "%s:   max_background          : %u\n", __func__, info->max_background);
	log(3, "%s:   congestion_threshold    : %u\n", __func__, info->congestion_threshold);
#endif
	return afs;
}

static int usage(const char* program)
{
	const char* prog = strrchr(program, '/');
	prog = prog ? prog + 1 : program;
	
	char dateStr[32];
	sprintf(dateStr, "%s %s", __DATE__, __TIME__);
	
	fprintf(stderr, "fuse-alto Version %s (%s) by Luca Severini <lucaseverini@mac.com>\n", FUSE_ALTO_VERSION, dateStr);
	fprintf(stderr, "Copyright (c) 2016, Juergen Buchmueller <pullmoll@t-online.de>\n\n");
	fprintf(stderr, "usage: %s <mountpoint> [options] <disk image file> [<second disk image file>]\n", prog);
	fprintf(stderr, "Where [options] can be one or more of\n");
	fprintf(stderr, "    -h|--help          prints this help and all possible options, then quits\n");
	fprintf(stderr, "    -d|--debug         prints debug messages\n");
	fprintf(stderr, "    -f|--foreground    runs fuse-alto in the foreground\n");
	fprintf(stderr, "    -s|--single        runs fuse-alto single threaded\n");
	fprintf(stderr, "    -v|--verbose       sets verbose mode (can be repeated)\n");
	fprintf(stderr, "    -c|--check         (not implemented yet) checks the validity of disk structure\n");
	fprintf(stderr, "    -r|--rebuild       (not implemented yet) rebuilds the disk structure like the scavenger programs does\n");
	fprintf(stderr, "    -V|--version       prints version of fuse and fuse-alto programs, then quits\n");
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
			if (nonopt_seen++ == 0)
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

		case KEY_CHECK:
			check = true;
			return 0;

		case KEY_REBUILD:
			rebuild = true;
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
		log(2, "%s: removing signal handlers\n", __func__);
		
		fuse_remove_signal_handlers(fuse_get_session(fuse));
	}
	
	if (mountpoint && chan)
	{
		log(2, "%s: unmounting %s\n", __func__, mountpoint);
		
		fuse_unmount(mountpoint, chan);
		mountpoint = 0;
		chan = 0;
	}
	
	if (fuse)
	{
		log(2, "%s: shutting down fuse\n", __func__);
		
		fuse_destroy(fuse);
		fuse = 0;
	}
	
	if (fuse_ops)
	{
		log(2, "%s: releasing fuse ops\n", __func__);
		
		free(fuse_ops);
		fuse_ops = 0;
	}
	
	if(fuse_args.allocated != 0)
	{
		log(2, "%s: releasing fuse args\n", __func__);
		
		fuse_opt_free_args(&fuse_args);
	}
}

int main(int argc, char *argv[])
{
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
		printf("%s pid:%d\n", basename(argv[0]), getpid());
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
			char dateStr[32];
			sprintf(dateStr, "%s %s", __DATE__, __TIME__);

			printf("fuse-alto Version %s (%s) by Luca Severini <lucaseverini@mac.com>\n", FUSE_ALTO_VERSION, dateStr);
			printf("Copyright (c) 2016, Juergen Buchmueller <pullmoll@t-online.de>\n");

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
	
	if (verbose > 0)
	{
		printf("verbosity: %d\n", verbose);
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
	if (chan == 0)
	{
		perror("fuse_mount()");
		exit(1);
	}
	
	fuse = fuse_new(chan, &fuse_args, fuse_ops, sizeof(*fuse_ops), NULL);
	if (fuse == 0)
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
	
	// printf("#### %d chars in buffer:\n", (int)size);
	
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
	// printf("\n#### %d chars in buffer:\n", (int)size);

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

