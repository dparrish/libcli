// vim:sw=4 ts=8 expandtab tw=100

#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef LIBCLI_THREADED
#include <pthread.h>
#endif
#include "stringbuffer.h"

StringBuffer *sb_new()
{
    StringBuffer *sb = calloc(sizeof(StringBuffer), 1);
    if (!sb) return NULL;
    sb->max = STRINGBUFFER_DEFAULT_MAX;
    sb->buf = (char *)malloc(STRINGBUFFER_DEFAULT_LEN);
    sb->allocated = STRINGBUFFER_DEFAULT_LEN;
#ifdef LIBCLI_THREADED
    pthread_mutex_init(&sb->mutex, NULL);
#endif
    return sb;
}

#ifdef LIBCLI_THREADED
void __sb_lock(StringBuffer *sb, char *file, int line) {
#ifdef STRINGBUFFER_DEBUG
    printf("_sb_lock(%p) %s:%d... ", sb, file, line);
    fflush(stdout);
#endif
    pthread_mutex_lock(&sb->mutex);
#ifdef STRINGBUFFER_DEBUG
    printf("done\n");
    fflush(stdout);
#endif
}
void __sb_unlock(StringBuffer *sb, char *file, int line) {
#ifdef STRINGBUFFER_DEBUG
    printf("_sb_unlock(%p) %s:%d\n", sb, file, line);
    fflush(stdout);
#endif
    pthread_mutex_unlock(&sb->mutex);
}
#define _sb_lock(sb) __sb_lock(sb, __FILE__, __LINE__)
#define _sb_unlock(sb) __sb_unlock(sb, __FILE__, __LINE__)

#else

#define _sb_lock(x) {}
#define _sb_unlock(x) {}

#endif

long sb_resize(StringBuffer *sb, long size)
{
    if (!(sb->buf = realloc(sb->buf, size)))
        return 0;
    _sb_lock(sb);
    sb->allocated = size;
    sb->head = sb->tail = 0;
    _sb_unlock(sb);
    return size;
}

void sb_destroy(StringBuffer *sb)
{
    if (!sb) return;
    if (!sb->buf) return;
    free(sb->buf);
#ifdef LIBCLI_THREADED
    pthread_mutex_destroy(&sb->mutex);
#endif
    memset(sb, 0, sizeof(StringBuffer));
    free(sb);
}

long sb_put(StringBuffer *sb, char *buf, long len)
{
    if (!sb || !buf || !len) return 0;
    _sb_lock(sb);
#ifdef STRINGBUFFER_DEBUG
    printf("Attempt to add %lu bytes to buffer: sb->allocated=%lu  sb->max=%lu  sb->head=%lu  sb->tail=%lu\n",
           len, sb->allocated, sb->max, sb->head, sb->tail);
#endif
    if (((sb->tail - sb->head) + len) > sb->max)
    {
        // Too large for this StringBuffer object
#ifdef STRINGBUFFER_DEBUG
        printf("That would be too large!\n");
#endif
        _sb_unlock(sb);
        return 0;
    }
    if ((sb->allocated - (sb->tail - sb->head)) < len)
    {
        // Not enough space available currently, but still allowed to get more
        sb->allocated *= 2;
#ifdef STRINGBUFFER_DEBUG
        printf("Need to realloc, new size is %lu\n", sb->allocated);
#endif
        if (!(sb->buf = realloc(sb->buf, sb->allocated)))
        {
            _sb_unlock(sb);
            return 0;
        }
    }
    if (sb->tail + len > sb->allocated)
    {
#ifdef STRINGBUFFER_DEBUG
        printf("Moving data to beginning of buffer\n");
#endif
        memmove(sb->buf, sb->buf + sb->head, (sb->tail - sb->head));
        sb->tail = (sb->tail - sb->head);
        sb->head = 0;
    }
    memcpy(sb->buf + sb->tail, buf, len);
    sb->tail += len;
    _sb_unlock(sb);
    return len;
}

long sb_put_string(StringBuffer *sb, char *buf)
{
    if (!sb || !buf || !*buf) return 0;
    return sb_put(sb, buf, strlen(buf) + 1);
}

long sb_peek(StringBuffer *sb, char *buf, long len)
{
    long ret = len;
    if (!sb || !buf || !len) return 0;
    _sb_lock(sb);
#ifdef STRINGBUFFER_DEBUG
    printf("sb_peek(%lu)  sb->head=%lu  sb->tail=%lu\n", len, sb->head, sb->tail);
#endif
    if ((sb->tail - sb->head) < ret)
        ret = (sb->tail - sb->head);
    memcpy(buf, sb->buf + sb->head, ret);
#ifdef STRINGBUFFER_DEBUG
    printf("Returned a read of %lu bytes\n", ret);
#endif
    _sb_unlock(sb);
    return ret;
}

long sb_get(StringBuffer *sb, char *buf, long len)
{
    long ret = len;
    if (!sb || !buf || !len) return 0;
    _sb_lock(sb);
#ifdef STRINGBUFFER_DEBUG
    printf("sb_get(%lu) sb->head=%lu  sb->tail=%lu\n", len, sb->head, sb->tail);
#endif
    if ((sb->tail - sb->head) < ret)
        ret = (sb->tail - sb->head);
    memcpy(buf, sb->buf + sb->head, ret);
    sb->head += ret;
    if (sb->tail <= sb->head)
    {
        sb->head = 0;
        sb->tail = 0;
    }
#ifdef STRINGBUFFER_DEBUG
    printf("sb_get() read %lu bytes\n", ret);
#endif
    _sb_unlock(sb);
    return ret;
}

long sb_get_string(StringBuffer *sb, char *buf, long len)
{
    long i, c = 0;
    if (!sb || !buf || !len) return 0;
    if (sb->head == sb->tail) return 0;
    _sb_lock(sb);
    for (i = sb->head; i < sb->tail; i++)
    {
        c++;
        if (*(sb->buf + i) == 0)
        {
            buf[c < len ? c : len] = 0;
            _sb_unlock(sb);
            return sb_get(sb, buf, c < len ? c : len);
        }
    }
    _sb_unlock(sb);
#ifdef STRINGBUFFER_DEBUG
    printf("sb_get_string() did not find a string\n");
#endif
    return 0;
}


long sb_len(StringBuffer *sb)
{
    if (!sb) return 0;
    return (sb->tail - sb->head);
}

void sb_empty(StringBuffer *sb)
{
    if (!sb) return;
    _sb_lock(sb);
    if ((sb->tail - sb->head) == 0 || !sb->buf) return;
    memset(sb->buf, 0, sb->allocated);
    sb->head = sb->tail = 0;
    _sb_unlock(sb);
}

void stringbuffer_test()
{
    char *src = "This is a string with some characters in it\n";
    char dest[150] = {0};
    StringBuffer *sb = sb_new();
    sb_resize(sb, 50);
    sb->max = 100;

    sb_put_string(sb, src);
    memset(dest, 0, 150);
    sb_peek(sb, dest, strlen(src));
    assert(strcmp(src, dest) == 0);
    memset(dest, 0, 150);
    sb_get_string(sb, dest, 149);
    assert(strcmp(src, dest) == 0);
    memset(dest, 0, 150);
    sb_get(sb, dest, strlen(src));
    assert(strcmp(src, dest));
    sb_put(sb, src, strlen(src));
    memset(dest, 0, 150);
    sb_get(sb, dest, strlen(src));
    assert(strcmp(src, dest) == 0);
    sb_put(sb, src, strlen(src));
    sb_put(sb, src, strlen(src));
    sb_put(sb, src, strlen(src));
    memset(dest, 0, 150);
    sb_get(sb, dest, strlen(src));
    assert(strcmp(src, dest) == 0);
    memset(dest, 0, 150);
    sb_get(sb, dest, strlen(src));
    assert(strcmp(src, dest) == 0);

    sb_destroy(sb);
}

