#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>
#include <mutex>
// io61.cc
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd = -1;     // file descriptor
    int mode;        // open mode (O_RDONLY or O_WRONLY)
    // `bufsiz` is the cache block size
    static constexpr off_t bufsize = 4096*16;
    // Cached data is stored in `cbuf`
    unsigned char cbuf[bufsize];

    // The following “tags” are addresses—file offsets—that describe the cache’s contents.
    // `tag`: File offset of first byte of cached data (0 when file is opened).
    off_t tag = 0;
    // `end_tag`: File offset one past the last byte of cached data (0 when file is opened).
    off_t end_tag = 0;
    // `pos_tag`: Cache position: file offset of the cache.
    // In read caches, this is the file offset of the next character to be read.
    off_t pos_tag = 0;
};


// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    std::lock_guard<std::recursive_mutex> lk(m);
   
    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    // Check if the next character is in the cache
    if (f->pos_tag < f->end_tag) {
        // Read the character from the cache
        unsigned char ch = f->cbuf[f->pos_tag - f->tag];
        f->pos_tag += 1;
        return ch;
    }

    // Cache miss, need to read more data into the cache
    f->tag = f->pos_tag;
    ssize_t nr = read(f->fd, f->cbuf, io61_file::bufsize);
    if (nr <= 0) {
        // Either an error occurred, or we hit EOF
        if (nr == 0) {
            errno = 0; // clear `errno` to indicate EOF
        } else {
            assert(nr == -1 && errno > 0);
        }
        return -1;
    }

    // Update cache tags
    f->end_tag = f->tag + nr;

    // Now the cache is refilled, return the next character
    unsigned char ch = f->cbuf[0];
    f->pos_tag += 1;
    return ch;
}

// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
size_t nread = 0; // Number of bytes read

    while (nread < sz) {
        // Check if there's data in the cache
        if (f->pos_tag < f->end_tag) {
            // Calculate the number of bytes to copy from the cache
            size_t ncache = std::min(sz - nread, static_cast<size_t>(f->end_tag - f->pos_tag));
            memcpy(buf + nread, f->cbuf + (f->pos_tag - f->tag), ncache);
            nread += ncache;
            f->pos_tag += ncache;
        }

        // If we still need more data, read from the file
        if (nread < sz) {
            f->tag = f->pos_tag;
            ssize_t nr = read(f->fd, f->cbuf, io61_file::bufsize);
            if (nr <= 0) {
                // Either an error occurred, or we hit EOF
                return nread > 0 ? nread : nr;
            }
            f->end_tag = f->tag + nr;
        }
    }

    return nread;
}


// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) {
    unsigned char ch = c;
    // If the cache is full, flush it
    if (f->pos_tag - f->tag >= io61_file::bufsize) {
        ssize_t nw = write(f->fd, f->cbuf, f->pos_tag - f->tag);
        if (nw <= 0) {
            // Either an error occurred, or couldn't write anything
            return -1;
        }
        f->tag = f->pos_tag = 0; // Reset cache
    }

    // Store the character in the cache
    f->cbuf[f->pos_tag - f->tag] = ch;
    f->pos_tag++;

    // Update end_tag
    if (f->pos_tag > f->end_tag) {
        f->end_tag = f->pos_tag;
    }

    return 0; // Success
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
   size_t nwritten = 0; // Number of bytes written

    while (nwritten < sz) {
        // Calculate space available in the cache
        size_t ncache = std::min(sz - nwritten, static_cast<size_t>(io61_file::bufsize - (f->pos_tag - f->tag)));

        // If the cache is full, flush it
        if (ncache == 0) {
            ssize_t nw = io61_flush(f);
            if (nw == -1) {
                // If flush failed and nothing was written, return -1
                // Otherwise, return the number of bytes written so far
                if (errno == EINTR || errno == EAGAIN) {
                    continue;
                }
                return nwritten > 0 ? nwritten : -1;
            }
            continue;
        }

        // Copy data into the cache
        memcpy(f->cbuf + (f->pos_tag - f->tag), buf + nwritten, ncache);
        nwritten += ncache;
        f->pos_tag += ncache;

        // Update end_tag
        if (f->pos_tag > f->end_tag) {
            f->end_tag = f->pos_tag;
        }
    }

    return nwritten;
}


// io61_flush(f)
//    If `f` was opened write-only, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. It may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {
    if (f->pos_tag > f->tag) {
        while (true) {
            ssize_t nw = write(f->fd, f->cbuf, f->pos_tag - f->tag);
            if (nw > 0) {
                // Move the unwritten part of the cache to the beginning
                memmove(f->cbuf, f->cbuf + nw, f->pos_tag - f->tag - nw);
                f->tag += nw;
                if (f->tag == f->pos_tag) { // Cache fully flushed
                    f->tag = f->pos_tag = f->end_tag = 0; // Reset cache
                    break;
                }
            } else if (errno != EINTR && errno != EAGAIN) {
                // Non-recoverable error
                return -1;
            }
            // If errno is EINTR or EAGAIN, retry the write
        }
    }
    return 0;
}
// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {
// Flush the cache if in write mode and cache is not empty
    if ((f->mode == O_WRONLY || f->mode == O_RDWR) && f->pos_tag != f->tag) {
        ssize_t nw = write(f->fd, f->cbuf, f->pos_tag - f->tag);
        if (nw <= 0) {
            // Handle write error
            return -1;
        }
        f->tag = f->pos_tag = f->end_tag = 0; // Reset cache
    }

    // Update cache state for read mode
    if ((f->mode == O_RDONLY || f->mode == O_RDWR) && off >= f->tag && off < f->end_tag) {
        // Seek within the cache
        f->pos_tag = off;
    } else {
        // Invalidate the cache since we are seeking outside of it
        f->tag = f->end_tag = f->pos_tag = off;
    }

    // Perform the actual seek
    off_t r = lseek(f->fd, off, SEEK_SET);
    if (r == -1) {
        // Handle seek error
        return -1;
    }

    return 0;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
