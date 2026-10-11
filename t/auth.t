use lib 't/lib';
use Test::LibCLI;
run_tests();

__DATA__

=== TEST 1: cli-aware login callback accepts, password not echoed
--- fixture: auth_ctx
--- input
alice
pw1
reload
--- output_like
^Username: alice$
^AUTH alice$
^Invalid command "reload"$
--- output_unlike
pw1
Access denied



=== TEST 2: denied login prompts for the username again
--- fixture: auth_ctx
--- input
alice
wrong
alice
pw1
--- output_like
Access denied
^AUTH alice$
^router>
--- output_unlike
wrong



=== TEST 3: CLI_QUIT from the login callback ends the session
--- fixture: auth_ctx
--- input
bob
x
bob
y
bob
z
alice
pw1
--- output_like
TOO MANY FAILURES
--- output_unlike
AUTH
^router>



=== TEST 4: cli-aware enable callback accepts
--- fixture: auth_ctx
--- input
alice
pw1
enable
en
reload
--- output_like
^ENABLE OK$
^router# reload$
^RAN reload$
--- output_unlike
^en$
Access denied



=== TEST 5: cli-aware enable callback denies
--- fixture: auth_ctx
--- input
alice
pw1
enable
nope
reload
--- output_like
Access denied
^router> reload$
^Invalid command "reload"$
--- output_unlike
nope



=== TEST 6: CLI_QUIT from the enable callback ends the session
--- fixture: auth_ctx
--- input
alice
pw1
enable
bye
reload
--- output_like
^ENABLE QUIT$
--- output_unlike
RAN reload
Invalid command



=== TEST 7: cli_read_password reads two hidden lines, kept out of history
--- fixture: auth_ctx
--- input
alice
pw1
secret
abc
xyz
history
--- output_like
^New password:\s*$
^Confirm password:\s*$
^RC1 0$
^RC2 0$
^GOT \[ABC\] \[XYZ\]$
--- output_unlike
abc
xyz



=== TEST 8: cli_read_password handles backspace and ctrl-u
--- fixture: auth_ctx
--- input
alice
pw1
secret
ab<DEL>c
zz<CTRL-U>q
--- output_like
^GOT \[AC\] \[Q\]$



=== TEST 9: ctrl-c cancels cli_read_password, the session goes on
--- fixture: auth_ctx
--- input
alice
pw1
secret
ab<CTRL-C>
reload
--- output_like
^RC1 -1$
^Invalid command "reload"$
--- output_unlike
RC2
GOT



=== TEST 10: a line too long for the buffer fails, and is read to its end
--- fixture: auth_ctx
--- input
alice
pw1
short
abcdef
short
abc
reload
--- output_like
^RC -1 LEN 0$
^RC 0 LEN 3$
^Invalid command "reload"$
--- output_unlike
abc
def



=== TEST 11: EOF while reading a password returns CLI_QUIT
--- fixture: auth_ctx
--- input eval
"alice\npw1\nsecret\nab"
--- output_like
^RC1 -2$
--- output_unlike
GOT



=== TEST 12: telnet options inside a password are skipped
--- fixture: auth_ctx
--- telnet
--- input eval
"alice\npw1\nsecret\na\xff\xfd\x03b\nc\xff\xfb\x01\n"
--- output_like
^GOT \[AB\] \[C\]$
