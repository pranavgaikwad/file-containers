// Project 3: Pranav Gaikwad, pmgaikwa

#include "fcfuse.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <fcontainer.h>

extern struct fcfuse_state *fcfuse_data;

int _is_directory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0) return 0;
   return S_ISDIR(statbuf.st_mode);
}

void _get_container_directory(char *root, int cid) 
{
    int length = snprintf(NULL, 0, "%d", cid);
    char* str = malloc(length + 11);
    snprintf(str, length + 11, "%s%d", ".container", cid);
    strncat(root, str, PATH_MAX);
}   

static void fcfuse_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, FCFS_DATA->rootdir);
    
    int cid = fcontainer_getcid(FCFS_DATA->devfd, fuse_get_context()->pid);

    strncat(fpath, path, PATH_MAX);

    int is_dir = _is_directory(fpath);

    if ((cid != -1) && !is_dir && (cid != NULL)) {
        _get_container_directory(fpath, cid);
    }
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int fcfuse_getattr(const char *path, struct stat *stbuf)
{
    char fpath[PATH_MAX];
    int retstat = -ENOENT;
        
    fcfuse_fullpath(fpath, path);
   
    retstat = lstat(fpath, stbuf);

    if (retstat == -1) return -errno;

    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to fcfuse_readlink()
// fcfuse_readlink() code by Bernardo F Costa (thanks!)
int fcfuse_readlink(const char *path, char *link, size_t size)
{
    char fpath[PATH_MAX];
    int retstat = -ENOENT;
    
    fcfuse_fullpath(fpath, path);

    retstat = readlink(fpath, link, size-1);
    
    if (retstat >= 0) {
       link[retstat] = '\0';
       retstat = 0;
    }

    if (retstat == -1) return -errno;
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int fcfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    char fpath[PATH_MAX];
    int retstat = -ENOENT;
    
    fcfuse_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
    if (S_ISREG(mode)) {
        retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (retstat >= 0) retstat = close(retstat);
    } else {
        if (S_ISFIFO(mode)) retstat = mkfifo(fpath, mode);
        else retstat = mknod(fpath, mode, dev);
    }

    if (retstat == -1) return -errno;

    return retstat;
}

/** Create a directory */
int fcfuse_mkdir(const char *path, mode_t mode)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = mkdir(fpath, mode);
    
    if (retstat == -1) return -errno;

    return 0;
}

/** Remove a file */
int fcfuse_unlink(const char *path)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = unlink(fpath);

    if (retstat == -1) return -errno;

    return 0;
}

/** Remove a directory */
int fcfuse_rmdir(const char *path)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = rmdir(fpath);

    if (retstat == -1) return -errno;

    return 0;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int fcfuse_symlink(const char *path, const char *link)
{
    int retstat;
    char flink[PATH_MAX];
    
    fcfuse_fullpath(flink, link);

    retstat = symlink(path, flink);

    if (retstat == -1) return -errno;

    return 0;
}

/** Rename a file */
// both path and newpath are fs-relative
int fcfuse_rename(const char *path, const char *newpath)
{
    int retstat;
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);
    fcfuse_fullpath(fnewpath, newpath);

    retstat = rename(fpath, fnewpath);

    if (retstat == -1) return -errno;

    return 0;
}

/** Create a hard link to a file */
int fcfuse_link(const char *path, const char *newpath)
{
    int retstat;
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);
    fcfuse_fullpath(fnewpath, newpath);

    retstat = link(fpath, fnewpath);

    if (retstat == -1) return -errno;

    return 0;
}

/** Change the permission bits of a file */
int fcfuse_chmod(const char *path, mode_t mode)
{
    int retstat = -ENOENT;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = chmod(fpath, mode);

    if (retstat == -1) return -errno;

    return retstat;
}

/** Change the owner and group of a file */
int fcfuse_chown(const char *path, uid_t uid, gid_t gid)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = chown(fpath, uid, gid);

    if (retstat == -1) return -errno;

    return 0;
}

/** Change the size of a file */
int fcfuse_truncate(const char *path, off_t newsize)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    retstat = truncate(fpath, newsize);

    if (retstat == -1) return -errno;

    return 0;
}

/** Change the access and/or modification times of a file */
int fcfuse_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];

    fcfuse_fullpath(fpath, path);

    retstat = utime(fpath, ubuf);

    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int fcfuse_open(const char *path, struct fuse_file_info *fi)
{
    int fd;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    fd = open(fpath, fi->flags);

    if (fd == -1) return -errno;
	
    fi->fh = fd;

    return retstat;

}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int fcfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
        
    retstat = pread(fi->fh, buf, size, offset);

    int cid = fcontainer_getcid(FCFS_DATA->devfd, fuse_get_context()->pid);
    if((cid != -1) && (cid != NULL)) fcontainer_delete(FCFS_DATA->devfd);

    if (retstat == -1) return -errno;

    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 */
int fcfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;

    retstat = pwrite(fi->fh, buf, size, offset);

    int cid = fcontainer_getcid(FCFS_DATA->devfd, fuse_get_context()->pid);
    if((cid != -1) && (cid != NULL)) fcontainer_delete(FCFS_DATA->devfd);

    if (retstat == -1) return -errno;
    
    return retstat;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int fcfuse_statfs(const char *path, struct statvfs *statv)
{
    int retstat;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = statvfs(fpath, statv);
    
    if (retstat == -1) return -errno;
    
    return 0;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int fcfuse_flush(const char *path, struct fuse_file_info *fi)
{	
    int retstat;

    retstat = close(dup(fi->fh));

    if (retstat == -1) return -errno;

    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int fcfuse_release(const char *path, struct fuse_file_info *fi)
{
    close(fi->fh);

    return 0;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int fcfuse_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
#ifdef HAVE_FDATASYNC
    if (datasync)
        return fdatasync(fi->fh);
    else
#endif
        retstat = fsync(fi->fh);
    if (retstat == -1) return -errno;
	return retstat;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int fcfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    return -61;
}

/** Get extended attributes */
int fcfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return getxattr(path, name, value, size);
}

/** List extended attributes */
int fcfuse_listxattr(const char *path, char *list, size_t size)
{
    return -61;
}

/** Remove extended attributes */
int fcfuse_removexattr(const char *path, const char *name)
{
    return -61;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 *
 * Introduced in version 2.3
 */
int fcfuse_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    fcfuse_fullpath(fpath, path);

    // since opendir returns a pointer, takes some custom handling of
    // return status.
    dp = opendir(fpath);

    if (dp == NULL) return -errno;
    
    fi->fh = (intptr_t) dp;    
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int fcfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    if (de == 0) return retstat;

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
    	if (filler(buf, de->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
    } while ((de = readdir(dp)) != NULL);
    
    return retstat;
}

/** Release directory
 */
int fcfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    retstat = closedir((DIR *) (uintptr_t) fi->fh);
    
    if (retstat == -1) return -errno;

    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? 
int fcfuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

int fcfuse_access(const char *path, int mask)
{
    int retstat;
    char fpath[PATH_MAX];
       
    fcfuse_fullpath(fpath, path);
    
    retstat = access(fpath, mask);
    
    if (retstat < 0) return -errno;
    
    return 0;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int fcfuse_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat;
    
    retstat = ftruncate(fi->fh, offset);
    
    if (retstat == -1) return -errno;

    return 0;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 */
int fcfuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/")) return fcfuse_getattr(path, statbuf);
    
    retstat = fstat(fi->fh, statbuf);
    
    // if (retstat < 0) return -errno;
        
    return retstat;
}

void *fcfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nfcfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    return FCFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void fcfuse_destroy(void *userdata)
{
    free(userdata);
}
