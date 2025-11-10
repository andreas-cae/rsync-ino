
## File stat:ing
Stat is only done in the `do_{f,l,}stat` functions in `syscall.c` that return `stat` or `stat64` structs, depending on if `USE_STAT64_FUNCS` has been set.

```c
int do_stat(const char *path, STRUCT_STAT *st);
int do_lstat(const char *path, STRUCT_STAT *st);
int do_fstat(int fd, STRUCT_STAT *st);

int do_stat(const char *path, STRUCT_STAT *st)
{
#ifdef USE_STAT64_FUNCS
	return stat64(path, st);
#else
	return stat(path, st);
#endif
}
```


```bash
ahall169@cs3-0091:~/git/rsync-ino_publ> grep -r 'stat64'
syscall.c:      return stat64(path, st);
syscall.c:      return lstat64(path, st);
syscall.c:      return fstat64(fd, st);
rsync.h:#define STRUCT_STAT struct stat64
getfsdev.c:             ret = stat64(*++argv, &st);
configure.ac:AC_HAVE_TYPE([struct stat64], [#include <stdio.h>
```

