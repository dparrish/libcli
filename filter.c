// vim:sw=4 ts=8 expandtab tw=100

#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "libcli.h"


cli_filter *filter_new(struct cli_def *cli, filter_callback callback)
{
    return filter_new_with_buffers(cli, callback, sb_new(), sb_new());
}

cli_filter *filter_new_with_buffers(struct cli_def *cli, filter_callback callback,
                                    StringBuffer *input, StringBuffer *output)
{
    cli_filter *filter = calloc(sizeof(cli_filter), 1);
    filter->input = input;
    filter->output = output;
    filter->cli = cli;
    filter->callback = callback;
    return filter;
}

cli_filter *filter_chain(cli_filter *tail, filter_callback callback)
{
    return filter_new_with_buffers(tail->cli, callback, tail->output, sb_new());
}

int filter_run(cli_filter *filter, void *user_state)
{
    char *in, *out;
    long in_len, out_len;
    int ret;
    if (!filter) return CLI_FILTER_ERROR;
    if (!filter->callback) return CLI_FILTER_ERROR;

    in = malloc(4096);
    out = malloc(4096);
    while (1)
    {
        in_len = sb_get_string(filter->input, in, 4095);
        if (in_len <= 0)
            break;
        in[in_len] = 0;
        out_len = 4096;
        ret = filter->callback(user_state, in, in_len, out, &out_len);
        if (out_len >= 0 || ret != 0)
            sb_put_string(filter->output, out);
    }

    free(in);
    free(out);

    return CLI_FILTER_END;
}

void filter_destroy(cli_filter *filter)
{
    if (!filter) return;
    sb_destroy(filter->input);
    sb_destroy(filter->output);
    free(filter);
}


int _filter_test_callback_1(void *user_state, const char *input, long input_len, char *output,
                            long *output_len)
{
    char *p;
    memcpy(output, input, input_len);
    output[input_len] = 0;
    if ((p = strstr(output, "foo")))
        memcpy(p, "bar", 3);

    *output_len = strlen(output);
    return 0;
}

int _filter_test_callback_2(void *user_state, const char *input, long input_len, char *output,
                            long *output_len)
{
    char *p;
    memcpy(output, input, input_len);
    output[input_len] = 0;
    if ((p = strstr(output, "bar")))
        memcpy(p, "foo", 3);

    *output_len = strlen(output);
    return 0;
}

void filter_test()
{
    char *input_lines[] = {
        "This is a line",
        "This is a foo line",
        "This another line",
    };
    char *buf;
    long n;
    int i;
    cli_filter *filter1, *filter2;

    // Create 2 filters, one to foo-ize, and one to de-fooize, chaining them together
    filter1 = filter_new(NULL, _filter_test_callback_1);
    filter2 = filter_chain(filter1, _filter_test_callback_2);

    // Add some junk text
    for (i = 0; i < sizeof(input_lines) / sizeof(input_lines[0]); i++)
        sb_put_string(filter1->input, input_lines[i]);

    // Run both filters
    while (sb_len(filter1->input) > 0)
    {
        filter_run(filter1, NULL);
        filter_run(filter2, NULL);
    }

    // Check that the output of the second filter matches the input to the first filter
    buf = malloc(4096);
    for (i = 0; ; i++)
    {
        if ((n = sb_get_string(filter2->output, buf, 4096)) <= 0)
            break;
        n = strcmp(buf, input_lines[i]);
        assert(n == 0);
    }
}

