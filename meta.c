#include "rsync.h"
#include "ifuncs.h"
#include "rounding.h"
#include "io.h"

/* TODO:

TODO: [X] Define meta_format in options.c and make it configurable via command line options
TODO: merge meta_write_filedata into meta_write_stats
TODO: Document format string options


Some notes on struct stat fields and types:

By POSIX standard: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys/stat.h.html:
the stat struct has the fields and types as follows:

	TYPE	  FIELD			Description							Bytes	POSIX requirement of type

    dev_t     st_dev;       // ID of device containing file        8    int
    ino_t     st_ino;       // inode number                        8   uint	ino_t shall be defined as unsigned integer types.
    mode_t    st_mode;      // protection                          4    int mode_t shall be an integer type.
    nlink_t   st_nlink;     // number of hard links                8    int nlink_t, uid_t, gid_t, and id_t shall be integer types
    uid_t     st_uid;       // user ID of owner                    4    int nlink_t, uid_t, gid_t, and id_t shall be integer types
    gid_t     st_gid;       // group ID of owner                   4    int nlink_t, uid_t, gid_t, and id_t shall be integer types
    dev_t     st_rdev;      // device ID (if special file)         8    int
    off_t     st_size;      // total size, in bytes                8   sint blkcnt_t and off_t shall be signed integer types.
    blksize_t st_blksize;   // blocksize for file system I/O       8   sint blksize_t, pid_t, and ssize_t shall be signed integer types.
    blkcnt_t  st_blocks;    // number of 512B blocks allocated     8   sint blkcnt_t and off_t shall be signed integer 
    time_t    st_atime;     // time of last access                 8    int time_t and clock_t shall be integer or real-floating types.
    time_t    st_mtime;     // time of last modification           8    int time_t and clock_t shall be integer or real-floating types.
    time_t    st_ctime;     // time of last status change          8    int time_t and clock_t shall be integer or real-floating types.


The stat64 struct is identical with the difference in type in the three fields:
	ino64_t st_ino;
	off64_t st_size;
	blkcnt64_t st_blocks;

In practice, this seems to be a distinction without difference as types.h on linux seems to define ino_t as __ino64_t

For portability to python struct packing/unpacking, we will assume the following sizes:
Field Name      Size [bytes]   Type for struct packing/unpacking
st_dev          8              uint64_t
st_ino          8              uint64_t
st_mode         4              uint32_t
st_nlink       8              uint64_t
st_uid         4              uint32_t
st_gid         4              uint32_t
st_rdev        8              uint64_t
st_size        8              int64_t
st_blksize     8              int64_t
st_blocks      8              int64_t
st_atime       8              int64_t
st_mtime       8              int64_t
st_ctime       8              int64_t
*/


extern int am_generator; 
extern int am_server;
extern int am_sender;
extern int am_root;
extern int am_daemon;
extern char * meta_log;
extern char * meta_str;
extern char * meta_fmt;
extern FILE * meta_fp;


static int meta_log_is_initiated = 0;



/*
void print_gen_rec(char *msg)
{
	rprintf(FWARNING, "%s am_generator %i, am_server %i, am_sender %i, am_root %i, am_daemon %i\n", msg, am_generator, am_server, am_sender, am_root, am_daemon);
}

static void DDWRITE()
{
	fprintf(meta_fp, ">");
	fwrite(&am_sender, sizeof(int), 1, meta_fp);
	fwrite(&am_generator, sizeof(int), 1, meta_fp);
	fwrite(&am_daemon, sizeof(int), 1, meta_fp);
	fwrite(&am_server, sizeof(int), 1, meta_fp);
	fwrite(&am_root, sizeof(int), 1, meta_fp);
	fprintf(meta_fp, "<");
}
*/

char get_filetype(mode_t mode)
{
	if (S_ISREG(mode)) return 'f';
	if (S_ISDIR(mode)) return 'd';
	if (S_ISLNK(mode)) return 'l';
	if (S_ISCHR(mode)) return 'c';
	if (S_ISBLK(mode)) return 'b';
	if (S_ISFIFO(mode)) return 'p';
	if (S_ISSOCK(mode)) return 's';
	return '?';
}



// Little-endian write functions
#ifdef WORDS_BIGENDIAN
/* BE system -> convert writes to LE */
static char b[8];
static int write_u64_le(FILE *fp, uint64_t v) 
{
    b[0] = (unsigned char)(v);
    b[1] = (unsigned char)(v >> 8);
    b[2] = (unsigned char)(v >> 16);
    b[3] = (unsigned char)(v >> 24);
    b[4] = (unsigned char)(v >> 32);
    b[5] = (unsigned char)(v >> 40);
    b[6] = (unsigned char)(v >> 48);
    b[7] = (unsigned char)(v >> 56);
    return fwrite(b, 1, 8, fp) == 8;
}

static int write_u32_le(FILE *fp, uint32_t v) 
{
	b[0] = (unsigned char)(v);
	b[1] = (unsigned char)(v >> 8);
	b[2] = (unsigned char)(v >> 16);
	b[3] = (unsigned char)(v >> 24);
	return fwrite(b, 1, 4, fp) == 4;
}

static int write_i64_le(FILE *fp, int64_t v) 
{
	b[0] = (unsigned char)(v);
	b[1] = (unsigned char)(v >> 8);
	b[2] = (unsigned char)(v >> 16);
	b[3] = (unsigned char)(v >> 24);
	b[4] = (unsigned char)(v >> 32);
	b[5] = (unsigned char)(v >> 40);
	b[6] = (unsigned char)(v >> 48);
	b[7] = (unsigned char)(v >> 56);
	return fwrite(b, 1, 8, fp) == 8;
}

static int write_i32_le(FILE *fp, int32_t v) 
{
	b[0] = (unsigned char)(v);
	b[1] = (unsigned char)(v >> 8);
	b[2] = (unsigned char)(v >> 16);
	b[3] = (unsigned char)(v >> 24);
	return fwrite(b, 1, 4, fp) == 4;
}
#else
/* Compiled on a LE system -> inline simple writes */
static inline int write_u64_le(FILE *fp, uint64_t v) { 
	return fwrite(&v, 1, 8, fp) == 8;
}
static inline int write_u32_le(FILE *fp, uint32_t v) {
	return fwrite(&v, 1, 4, fp) == 4;
}
static inline int write_i64_le(FILE *fp, int64_t v) {
	return fwrite(&v, 1, 8, fp) == 8;
}
static inline int write_i32_le(FILE *fp, int32_t v) {
	return fwrite(&v, 1, 4, fp) == 4;
}
#endif


static int write_str(FILE *fp, const char *s) 
{
	size_t len = strlen(s); // exclude null terminator
	return (write_u32_le(fp, (uint32_t)len) && (fwrite(s, 1, len, fp) == len));
}


static uint32_t meta_file_version = 1; /* current version */
static const char * meta_file_type = "RSYNC-INO";

/** Write header to file
 * Header format (version 1):
 * 	    STRING - meta_file_type  (RSYNC-INO)
 *     	UINT32 - meta_file_version (file version, = 1)
 *     	STRING - meta_str
 *     	STRING - meta_fmt
*/
static void meta_write_header(void)
{
	if (!meta_fp) {
		rprintf(FERROR, "Metadata file is not opened for writing header.\n");
		exit_cleanup(RERR_FILEIO);
	}

	int SUCCESS = 0;
	switch (meta_file_version) {
		case 1:
			SUCCESS = write_str(meta_fp, meta_file_type)
					&& write_u32_le(meta_fp, meta_file_version) 
					&& write_str(meta_fp, meta_str)
					&& write_str(meta_fp, meta_fmt);
			break;
		default:
			rprintf(FERROR, "Unsupported metadata file version: %d\n", meta_file_version);
			exit_cleanup(RERR_FILEIO);
	}
	
	if (!SUCCESS) {
		rprintf(FERROR, "Failed to write metadata file header.\n");
		exit_cleanup(RERR_FILEIO);
	}	
}


void meta_open_file(void)
{
	if (meta_fp != NULL || meta_log == NULL) 
		return;

	if (meta_log_is_initiated) {
		meta_fp = fopen(meta_log, "ab");
	} else {
		meta_fp = fopen(meta_log, "wb");
		if (meta_fp)
			meta_write_header();
		meta_log_is_initiated = 1;
	}
	if (meta_fp == NULL) {
		rprintf(FERROR, "Failed to open metadata file: %s\n", meta_log);
		exit_cleanup(RERR_FILEIO);
	}
	return;
}


void meta_close_file(void)
{
	if (meta_fp != NULL) {
		fflush(meta_fp);
		fclose(meta_fp);
		meta_fp = NULL;
	}
}


/**
Write file metadata to the specified file pointer 
The stat fields and order is given by the string fmt, where each character represents a field to write
	N - fname
	d - st_dev
	i - st_ino
	M - st_mode
	n - st_nlink
	u - st_uid
	g - st_gid
	r - st_rdev
	s - st_size
	B - st_blksize
	b - st_blocks
	a - st_atime
	c - st_ctime
	m - st_mtime
*/
void meta_write_stats(const char *fname, STRUCT_STAT *st)
{
	if (!am_sender || am_server || meta_log == NULL) 
		return;

	meta_open_file();

	int SUCCESS = 1;
	for (const char *p = meta_fmt; *p != '\0'; p++) {
		switch (*p) {
			case 'N': 	SUCCESS = write_str(meta_fp, fname); break;
			case 'd':   SUCCESS = write_u64_le(meta_fp, (uint64_t)st->st_dev); break;
			case 'i':   SUCCESS = write_u64_le(meta_fp, (uint64_t)st->st_ino); break;
			case 'T': ;  char filetype = get_filetype(st->st_mode);
						SUCCESS = fwrite(&filetype, sizeof(char), 1, meta_fp) == 1; break;
			case 'M':   SUCCESS = write_u32_le(meta_fp, (uint32_t)st->st_mode); break;
			case 'n':   SUCCESS = write_u64_le(meta_fp, (uint64_t)st->st_nlink); break;
			case 'u':   SUCCESS = write_u32_le(meta_fp, (uint32_t)st->st_uid); break;
			case 'g':   SUCCESS = write_u32_le(meta_fp, (uint32_t)st->st_gid); break;
			case 'r':   SUCCESS = write_u64_le(meta_fp, (uint64_t)st->st_rdev); break;
			case 's':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_size); break;
			case 'B':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_blksize); break;
			case 'b':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_blocks); break;
			case 'a':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_atime); break;
			case 'c':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_ctime); break;
			case 'm':   SUCCESS = write_i64_le(meta_fp, (int64_t)st->st_mtime); break;
			default:
				rprintf(FERROR, "Unknown format specifier: %c\n", *p);
				SUCCESS = 0;
		}

		if (!SUCCESS) {
			rprintf(FERROR, "Failed to write metadata for file: '%s'\n", fname);
			exit_cleanup(RERR_FILEIO);
		}
	}
	return;
}
