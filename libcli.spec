Version: 1.9.7
Summary: Cisco-like telnet command-line library
Name: libcli
Release: 1
License: LGPL
Group: Library/Communication
Source: %{name}-%{version}.tar.gz
URL: http://code.google.com/p/libcli
Packager: David Parrish <david@dparrish.com>
BuildRoot: %{_tmppath}/%{name}-%{version}-%(%__id -un)
%description
libcli provides a shared library for including a Cisco-like command-line
interface into other software. It's a telnet interface which supports
command-line editing, history, authentication and callbacks for a
user-definable function tree.

%prep
%setup

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT PREFIX=/usr install
find $RPM_BUILD_ROOT/usr ! -type d -print | grep -v '\/(README|\.html)$' | \
    sed "s@^$RPM_BUILD_ROOT@@g" | sed "s/^\(.*\)$/\1\*/" > %{name}-%{version}-filelist

%post
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}-%{version}-filelist
%defattr(-, root, root)

%changelog
* Mon Feb  1 2010 David Parrish <david@dparrish.com> 1.9.7-1
- Fix memory leak in cli_get_completions - fengxj325@gmail.com

* Tue Jun  5 2012 Teemu Karimerto <teemu.karimerto@steo.fi> 1.9.6-1
- Added a user-definable context to struct cli_def
- Added cli_set_context/cli_get_context for user context handling
- Added a test for user context

* Mon Feb  1 2010 David Parrish <david@dparrish.com> 1.9.5-1
- Removed dependence on "quit" command
- Added cli_set_idle_timeout_callback() for custom timeout handling
- Fixed an error caused by vsnprintf() overwriting it's input data
- Added #ifdef __cplusplus which should allow linking with C++ now

* Thu Oct  9 2008 David Parrish <david@dparrish.com> 1.9.4-1
- cli_regular() failures now close client connections
- Migrate development to Google Code
- Remove docs as they were out of date and now migrated to Google Code wiki

* Fri Jul 28 2008 David Parrish <david@dparrish.com> 1.9.3-1
- Add support for compiling on WIN32 (Thanks Hamish Coleman)
- Fix cli_build_shortest() length handling
- Don't call cli_build_shortest() when registering every command
- Disable TAB completion during username entry

* Fri Jun  2 2008 David Parrish <david@dparrish.com> 1.9.2-1
- Add configurable timeout for cli_regular() - defaults to 1 second
- Add idle timeout support

* Thu Jul  5 2007 Brendan O'Dea <bod@optus.net> 1.9.1-1
- Revert callback argument passing to match 1.8.x
- Recalculate unique_len on change of priv/mode
- Fixes for tab completion

* Thu Jun 07 2007 David Parrish <david@dparrish.com> 1.9.0-1
- Implemented tab completion - Thanks Marc Donner, Andrew Silent, Yuriy N. Shkandybin and others
- Filters are now extendable
- Rename internal functions to all be cli_xxxx()
- Many code cleanups and optimisations
- Fix memory leak calling cli_loop() repeatedly - Thanks Qiang Wu

* Mon Jan 19 2007 David Parrish <david@dparrish.com> 1.8.8-1
- Fix broken auth_callback logic - Thanks Ben Menchaca

* Sat Jun 17 2006 Brendan O'Dea <bod@optus.net> 1.8.7-1
- Code cleanups.
- Declare internal functions static.
- Use private data in cli_def rather than static buffers for do_print
  and command_name functions.

* Mon Mar 06 2006 David Parrish <david@dparrish.com> 1.8.6-1
- Fix file descriptor leak in cli_loop() - Thanks Liam Widdowson
- Fix memory leak when calling cli_init() and cli_done() repeatedly.

* Fri Nov 25 2005 Brendan O'Dea <bod@optus.net> 1.8.5-2
- Apply spec changes from Charlie Brady: use License header, change
  BuildRoot to include username.

* Mon May  2 2005 Brendan O'Dea <bod@optusnet.com.au> 1.8.5-1
- Add cli_error function which does not filter output.

* Wed Jan  5 2005 Brendan O'Dea <bod@optusnet.com.au> 1.8.4-1
- Add printf attribute to cli_print prototype

* Fri Nov 19 2004 Brendan O'Dea <bod@optusnet.com.au> 1.8.3-1
- Free help if set in cli_unregister_command (reported by Jung-Che Vincent Li)
- Correct auth_callback() documentation (reported by Serge B. Khvatov)

* Thu Nov 11 2004 Brendan O'Dea <bod@optusnet.com.au> 1.8.2-1
- Allow config commands to exit a submode
- Make "exit" work in exec/config/submodes
- Add ^K (kill to EOL)

* Mon Jul 12 2004 Brendan O'Dea <bod@optusnet.com.au> 1.8.1-1
- Documentation update.
- Allow NULL or "" to be passed to cli_set_banner() and
  cli_set_hostname() to clear a previous value.

* Sun Jul 11 2004 Brendan O'Dea <bod@optusnet.com.au> 1.8.0-1
- Dropped prompt arg from cli_loop now that prompt is set by
  hostname/mode/priv level; bump soname.  Fixes ^L and ^A.
- Reworked parsing/filters to allow multiple filters (cmd|inc X|count).
- Made "grep" use regex, added -i, -v and -e args.
- Added "egrep" filter.
- Added "exclude" filter.

* Fri Jul  2 2004 Brendan O'Dea <bod@optusnet.com.au> 1.7.0-1
- Add mode argument to cli_file(), bump soname.
- Return old value from cli_set_privilege(), cli_set_configmode().

* Fri Jun 25 2004 Brendan O'Dea <bod@optusnet.com.au> 1.6.2-1
- Small cosmetic changes to output.
- Exiting configure/^Z shouldn't disable.
- Support encrypted password.

* Fri Jun 25 2004 David Parrish <david@dparrish.com> 1.6.0
- Add support for privilege levels and nested config levels. Thanks to Friedhelm
  Düsterhöft for most of the code.

* Tue Feb 24 2004 David Parrish <david@dparrish.com>
- Add cli_print_callback() for overloading the output
- Don't pass around the FILE * handle anymore, it's in the cli_def struct anyway
- Add cli_file() to execute every line read from a file handle
- Add filter_count

* Sat Feb 14 2004 Brendan O'Dea <bod@optusnet.com.au> 1.4.0-1
- Add more line editing support: ^W, ^A, ^E, ^P, ^N, ^F, ^B
- Modify cli_print() to add \r\n and to split on \n to allow inc/begin
  to work with multi-line output (note:  API change, client code
  should not include trailing \r\n; version bump)
- Use libcli.so.M.m as the soname

* Fri Jul 25 2003 David Parrish <david@dparrish.com>
- Add cli_regular to enable regular processing while cli is connected

* Wed Jun 25 2003 David Parrish <david@dparrish.com>
- Stop random stack smashing in cli_command_name.
- Stop memory leak by allocating static variable in cli_command_name.
