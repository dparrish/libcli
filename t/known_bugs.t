use lib 't/lib';
use Test::LibCLI;

# Bugs that do not belong to a single topical test file.  Unfixed ones
# are marked SKIP so the suite stays green; drop the "--- SKIP" line (and
# the "(KNOWN BUG ...)" note) as part of the fix to turn the block into a
# live regression test.
run_tests();

__DATA__

=== TEST 1: unregistering a child command leaves the parent usable
--- fixture: unreg_child
--- input
show ?
show
--- output_like
^No callback for "show"$
--- output_unlike
counters



=== TEST 2: telnet subnegotiation payload must not leak into the command line (KNOWN BUG libcli.c:1251)
--- SKIP
--- telnet: 1
--- input eval
"\xff\xfa\x00ZZZ\xff\xf0show version\r"
--- output_like
^RAN show version$
--- output_unlike
Invalid command "ZZZshow"



=== TEST 3: registering a duplicate optarg leaves a usable cli
--- fixture: dupoptarg
--- input
set hello
--- output_like
^RAN set$
^OPTARG name=hello$
