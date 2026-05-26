#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static const char *source_dir = "/home/bagas/reports";
static const char *prefix = "LAPORAN_";

/*
    Mengubah path dari mount point
    menjadi path asli di source directory
*/
void get_real_path(char fpath[1024], const char *path)
{
    char temp[1024];

    strcpy(temp, path);

    // hapus "/" di depan
    if (temp[0] == '/')
        memmove(temp, temp + 1, strlen(temp));

    // hapus prefix LAPORAN_
    if (strncmp(temp, prefix, strlen(prefix)) == 0)
    {
        memmove(temp,
                temp + strlen(prefix),
                strlen(temp) - strlen(prefix) + 1);
    }

    sprintf(fpath, "%s/%s", source_dir, temp);
}

/*
    Mengecek apakah file berekstensi .txt
*/
int is_txt_file(const char *name)
{
    const char *ext = strrchr(name, '.');

    if (!ext)
        return 0;

    return strcmp(ext, ".txt") == 0;
}

/*
    getattr
*/
static int x_getattr(const char *path,
                     struct stat *stbuf,
                     struct fuse_file_info *fi)
{
    char fpath[1024];

    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    // root directory
    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    get_real_path(fpath, path);

    // hanya file .txt yang boleh diakses
    if (!is_txt_file(fpath))
        return -ENOENT;

    if (lstat(fpath, stbuf) == -1)
        return -errno;

    return 0;
}

/*
    readdir
*/
static int x_readdir(const char *path,
                     void *buf,
                     fuse_fill_dir_t filler,
                     off_t offset,
                     struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    dp = opendir(source_dir);

    if (dp == NULL)
        return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL)
    {
        // skip selain file txt
        if (!is_txt_file(de->d_name))
            continue;

        char displayed_name[1024];

        sprintf(displayed_name,
                "%s%s",
                prefix,
                de->d_name);

        filler(buf, displayed_name, NULL, 0, 0);
    }

    closedir(dp);

    return 0;
}

/*
    open
*/
static int x_open(const char *path,
                  struct fuse_file_info *fi)
{
    char fpath[1024];

    // read-only
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;

    get_real_path(fpath, path);

    if (!is_txt_file(fpath))
        return -ENOENT;

    int fd = open(fpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    close(fd);

    return 0;
}

/*
    read
*/
static int x_read(const char *path,
                  char *buf,
                  size_t size,
                  off_t offset,
                  struct fuse_file_info *fi)
{
    char fpath[1024];

    (void) fi;

    get_real_path(fpath, path);

    if (!is_txt_file(fpath))
        return -ENOENT;

    int fd = open(fpath, O_RDONLY);

    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);

    if (res == -1)
        res = -errno;

    close(fd);

    return res;
}

/*
    Semua operasi write ditolak
*/

static int x_write()
{
    return -EROFS;
}

static int x_create()
{
    return -EROFS;
}

static int x_unlink()
{
    return -EROFS;
}

static int x_mkdir()
{
    return -EROFS;
}

static int x_rmdir()
{
    return -EROFS;
}

static struct fuse_operations x_oper = {
    .getattr = x_getattr,
    .readdir = x_readdir,
    .open = x_open,
    .read = x_read,

    .write = (void *) x_write,
    .create = (void *) x_create,
    .unlink = (void *) x_unlink,
    .mkdir = (void *) x_mkdir,
    .rmdir = (void *) x_rmdir,
};

int main(int argc, char *argv[])
{
    umask(0);
    return fuse_main(argc, argv, &x_oper, NULL);
}