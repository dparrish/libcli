// vim:sw=4 ts=8 expandtab tw=100

#ifndef __STRINGBUFFER_H__
#define __STRINGBUFFER_H__


typedef struct {
    char *buf;
    long head;
    long tail;
    long allocated;
    long max;
#ifdef LIBCLI_THREADED
    pthread_mutex_t mutex;
#endif
} StringBuffer;

#define STRINGBUFFER_DEFAULT_LEN 1024
#define STRINGBUFFER_DEFAULT_MAX 65536

StringBuffer *sb_new();
void sb_destroy(StringBuffer *sb);
long sb_resize(StringBuffer *sb, long size);
long sb_put(StringBuffer *sb, char *buf, long len);
long sb_put_string(StringBuffer *sb, char *buf);
long sb_peek(StringBuffer *sb, char *buf, long len);
long sb_get(StringBuffer *sb, char *buf, long len);
long sb_get_string(StringBuffer *sb, char *buf, long len);
long sb_len(StringBuffer *sb);
void sb_empty(StringBuffer *sb);

void stringbuffer_test();

#endif
