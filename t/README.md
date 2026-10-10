# libcli test suite

Data-driven tests for libcli, in the style of
[Test::Nginx](https://metacpan.org/dist/Test-Nginx). Each test is a block
of text describing input to type into a running CLI session and the
output to expect back. The library is compiled directly into a small C
driver; built with sanitizers (`USE_ASAN=1`) or run under valgrind,
memory and undefined-behaviour errors turn into test failures.

## Running

    make test              # plain driver
    USE_ASAN=1 make test   # AddressSanitizer + UndefinedBehaviorSanitizer

`make test` builds `t/cli_driver` and runs `prove -r t/`. With
`USE_ASAN=1` the driver is built with `-fsanitize=address,undefined`.
The driver is not rebuilt when only `USE_ASAN` changes, so `rm
t/cli_driver` (or `make clean`) when switching. The same blocks can also
be run under valgrind; see [valgrind mode](#valgrind-mode).

Run one file verbosely:

    prove -v t/filters.t

To run a single block while debugging, add `--- ONLY` to it (Test::Base
runs only that block); remove it before committing.

Requires Perl with `Test::Base` (Debian/Ubuntu: `libtest-base-perl`).

## Layout

    t/
      cli_driver.c       the C driver: builds a CLI from a fixture, runs cli_loop()
      lib/Test/LibCLI.pm the harness: forks the driver, feeds input, checks output
      valgrind.supp      baseline suppressions for known, pre-existing leaks
      basic.t            commands, auth, enable, config mode, history
      editing.t          line-editing keys (ctrl-a/e/b/f/k/w/u/z, backspace, insert)
      filters.t          include/exclude/count/begin/between/grep/egrep
      completion.t       tab and '?' completion, validators
      known_bugs.t       cross-cutting bug blocks (fixed ones live, unfixed SKIP)

## How a block runs

For each block the harness:

1. creates a `socketpair` and forks the driver, with one socket end as the
   driver's stdin/stdout (what `cli_loop()` reads and writes), and the
   driver's stderr captured to a temp file;
2. types the block's `input` into the socket, then closes the write end;
3. reads all output until EOF, with an `alarm`-based timeout guard;
4. renders that byte stream the way a terminal would (see **Rendering**);
5. checks the block's assertions, plus three implicit ones that run on
   **every** block: the driver must finish within the timeout, must not die
   from a signal, and must not print a sanitizer or valgrind report.

Each block is one `Test::More` subtest, so a failure names the block and
shows the full session output and stderr as diagnostics.

## Block sections

A block starts with `=== NAME` and contains `--- section` entries:

| Section | Meaning |
|---------|---------|
| `--- fixture: NAME` | driver fixture to build (default `basic`) |
| `--- users` | one `name:password` per line; enables login |
| `--- enable_password: PW` | password for the `enable` command |
| `--- hostname: NAME` | hostname shown in the prompt (default `router`) |
| `--- telnet` | enable telnet protocol negotiation (presence only) |
| `--- input` | keystrokes; each newline is sent as Enter (`\r`) |
| `--- input eval` | Perl that returns the input string (for generated input) |
| `--- output` | exact rendered output must equal this |
| `--- output_like` | one regex per line; each must match |
| `--- output_unlike` | one regex per line; none may match |
| `--- exit_code: N` | driver exit code must be N (default 0) |
| `--- timeout: SECONDS` | per-block timeout (default 10; scaled under valgrind) |
| `--- SKIP` | Test::Base: exclude this block from the run (see below) |

Regexes are matched in `/ms` mode, so `^` and `$` anchor to line
boundaries within the rendered output.

### Input key markers

Inside `--- input`, these stand for control bytes:

    <TAB> <ESC> <DEL> <UP> <DOWN> <LEFT> <RIGHT>   arrow keys send ANSI sequences
    <CTRL-X>   any letter, e.g. <CTRL-U> clears the line
    <LT>       a literal "<"

`--- input eval` is Perl; use it for input a literal block cannot express,
e.g. a near-buffer-length line or raw bytes:

    --- input eval
    "show" . (" " x 4091) . "<TAB>\n"

## Rendering

libcli's line editor redraws with backspaces and carriage returns, so the
raw bytes on the socket are not what a user sees. Before checking output
the harness replays the stream as a dumb terminal would: `\b` moves the
cursor left, characters overwrite, `\r` returns to column 0, `\a` (bell)
is dropped, and trailing spaces on each line are removed. Assertions
therefore describe the *visible* screen, not the redraw bytes.

## The driver and fixtures

`t/cli_driver.c` builds a CLI from a named fixture, then calls
`cli_loop()` on fd 0. Generic command callbacks print what they received
so tests can assert on how a line was parsed:

    RAN <full command name>
    OPTARG <name>=<value>
    ARG[<n>] <value>

Fixtures currently available (`--- fixture:`):

| Fixture | Purpose |
|---------|---------|
| `basic` | subcommands, optargs, privilege, config mode, output for filters |
| `completion` | a single long child plus common-prefix children |
| `validate` | a command with a validated argument |
| `dupoptarg` | registers a duplicate optarg, then exercises the command |
| `buildmode` | a build-mode command whose optarg name collides with a reserved one |
| `unreg_child` | registers `show counters`, then unregisters the child |

### Adding a fixture

Add a `drv_fixture_*` function in `t/cli_driver.c`, list it in
`drv_fixtures[]`, and forward-declare it. Fixtures return `CLI_OK` or
`CLI_ERROR`. The driver also accepts `--user name:password`, `--enable
PW`, `--hostname NAME`, and `--telnet`, which the harness sets from the
corresponding block sections.

## Known bugs: `--- SKIP`

A block documenting a bug that is not yet fixed is marked with
Test::Base's `--- SKIP`, which removes it from `blocks()` so it does not
run and the suite stays green. Its title ends with `(KNOWN BUG
libcli.c:LINE)`. The block states the *correct* expected behaviour, so
fixing the bug is: delete the `--- SKIP` line (and the `(KNOWN BUG ...)`
note), and the block becomes a live regression test.

Because `SKIP` blocks never run, fixing a bug will not automatically flag
that its block now passes — removing the `--- SKIP` line is a required
step of the fix.

Current known-bug blocks:

| Block | libcli.c | Symptom |
|-------|----------|---------|
| basic.t "password may contain '?'" | 1544 | `?` swallowed in password states |
| basic.t "password mid-line edit" | 1358 | cursor moved without compacting buffer |
| known_bugs.t "unregister child" | 476 | parent `children` dangles (use-after-free) |
| known_bugs.t "telnet subnegotiation" | 1251 | no SB…SE state machine; payload injected |

## valgrind mode

Set `LIBCLI_VALGRIND=1` to run every block under Memcheck. ASan and
valgrind cannot share a binary, so use the default (non-ASan) driver:

    make
    LIBCLI_VALGRIND=1 prove -r t/

Memcheck catches two things the ASan build does not: reads of uninitialized memory, and leaks
(the ASan build runs with `detect_leaks=0`). Each block's timeout is
scaled up automatically because Memcheck is ~20–30x slower.

`t/valgrind.supp` baselines known pre-existing leaks so the gate still
fails on *new* ones. Each suppression names the bug it hides; remove the
suppression as part of the fix so the exercising test turns into live
leak coverage. It currently suppresses one leak: `cli_done()` does not
free `cli->enable_password` (allocated in `cli_allow_enable`,
libcli.c:270).

## CI

`USE_ASAN=1 make test` is suitable for every push (seconds). The valgrind run
suits a nightly or pre-release job. Both need `libtest-base-perl`;
valgrind additionally needs the `valgrind` package.
