// vim:sw=4 ts=8 expandtab tw=100

#ifndef __FILTER_H__
#define __FILTER_H__


#include "libcli.h"
#include "stringbuffer.h"

enum cli_filter_return {
  CLI_FILTER_OK,
  CLI_FILTER_END,
  CLI_FILTER_ERROR,
};

struct _cli_filter;
typedef int (*filter_callback)(void *user_state, const char *input, long input_len, char *output,
                               long *output_len);

typedef struct _cli_filter {
  StringBuffer *input;
  StringBuffer *output;
  struct cli_def *cli;
  void *state;
  filter_callback callback;
} cli_filter;


cli_filter *filter_new(struct cli_def *cli, filter_callback callback);
cli_filter *filter_new_with_buffers(struct cli_def *cli, filter_callback callback,
                                    StringBuffer *input, StringBuffer *output);
int filter_run(cli_filter *filter, void *user_state);
void filter_destroy(cli_filter *filter);
void filter_test();

#endif
