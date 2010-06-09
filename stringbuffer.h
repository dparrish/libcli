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
long sb_peek(StringBuffer *sb, char *buf, long len);
long sb_get(StringBuffer *sb, char *buf, long len);
long sb_get_line(StringBuffer *sb, char *buf, long len);
long sb_len(StringBuffer *sb);
void sb_empty(StringBuffer *sb);

void stringbuffer_test();
