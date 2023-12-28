Version: 1.10.8
Summary: Cisco-like telnet command-line library
Name: libcli
Release: 1
License: LGPL
Group: Library/Communication
Source: %{name}-%{version}.tar.gz
URL: http://code.google.com/p/libcli
Packager: David Parrish <david@dparrish.com>
BuildRoot: %{_tmppath}/%{name}-%{version}-%(%__id -un)

%define verMajMin %(echo %{version} | cut -d '.' -f 1,2)

%package devel
Summary: Development files for libcli
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description
libcli provides a shared library for including a Cisco-like command-line
interface into other software. It's a telnet interface which supports
command-line editing, history, authentication and callbacks for a
user-definable function tree.

%description devel
libcli provides a shared library for including a Cisco-like command-line
interface into other software. It's a telnet interface which supports
command-line editing, history, authentication and callbacks for a
user-definable function tree.
This package contains the devel files.

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
install -d -p %{buildroot}%{_includedir}
install -p -m 644 libcli*.h %{buildroot}%{_includedir}/
install -d -p %{buildroot}%{_libdir}
install -p -m 755 libcli.so.%{version} %{buildroot}%{_libdir}/
install -p -m 755 libcli.a %{buildroot}%{_libdir}/

cd %{buildroot}%{_libdir}
ln -s libcli.so.%{version} libcli.so.%{verMajMin}
ln -s libcli.so.%{verMajMin} libcli.so

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc COPYING
%{_libdir}/*\.so.*
%defattr(-, root, root)

%files devel
%doc README.md doc/developers-guide.md
%{_libdir}/*.so*
%{_libdir}/*.a
%{_includedir}/*.h
%defattr(-, root, root)

%changelog
* Wed Dec 27 2023 Rob Sanders <rsanders@forcepointgov.com> 1.10.8
- Replace strchrnul() with possibly 2 calls to strchr() (issue #78)

* Sun Dec 17 2023 Rob Sanders <rsanders@forcepointgov.com> 1.10.8
- Changed from strcasecmp to strcmp in cli_loop() to do case sensitive comparison for <tab> completion (issue #91)

* Fri Dec 2 2022 Rob Sanders <rsanders@forcepointgov.com> 1.10.8
- Added regular callback fixes to ensure callback fires every # seconds regardless of input - code provided by github user JereLeppanen initially on 27Apr2022(issue #76)

* Thu Dec 1 2022 Rob Sanders <rsanders@forcepointgov.com> 1.10.8
- Add backward compatibility for LIBCLI versions numbers (issue #85)
- applied fixes for misspellings on LIBCLI version #defines (issue #75) - fix provided on github by belge-sel on 19Sep2021 
- Fix for printing (issue #79) where text left in buffer (from call to cli_bufprint for example) is discarded instead of preserved.  Code provided on github by JereLeppanen on 27Apr2021

* Wed Nov 16 2022 Rob Sanders <rsanders@forcepointgov.com> 1.10.8
- Doxygen headers for libli.c routines - code provided on github by mpzanoosi on 14May2021

* Wed Feb 24 2021 Rob Sanders <rsanders.forcepoint.com> 1.10.7
- Fix bug were an extra newline was being inserted on every line
  when help was being requested for options and arguments
- Fix memory leak in linewrapping code for help items

* Mon Feb 22 2021 Rob Sanders <rsanders.forcepoint.com> 1.10.6
- Fix bug when a command not found in the current mode, but is found
  in the CONFIG_MODE, which would resultin an an infinate loop.  Bug
  reported by Stefan Mächler (github @machste).

* Wed Jan 27 2021 Gerrit Huizenga <gerrit@us.ibm.com> 1.10.5
- Add ppc64le to travis builds

* Wed Jan 27 2021 Rob Sanders <rsanders@forcepoint.com> 1.10.5
- Add additional range chack to cli_loop() if the 'select' call is used, and
  punt if sockfd is out of range
- Add preprocessor check (LIBCLI_USE_POLL) to toggle between using poll or
  select in cli_loop().  Code submitted on github by @belge-sel.
- Fix possible error where cli_command_name() returns a NULL by
  generating full command name when a command is registered. 
  Note - removed cli->commandname member

* Thu Jan 14 2021 Rob Sanders <rsanders@forcepoint.com> 1.10.5
- Fix issue where the help for 'long command name' winds up running into
  the actual help text w/o any spaces.  Now a long command will be on a line
  by itself with the line having the indented help text.
- Change error notification of command processing to make it clearer where
  an error was found when processing a command, rather than displaying
  all of the command elements from the offending word back to the top 
  level command. 
- Fix incorrect internal version defined in libcli.h for the 1.10.4 release
- Minor change in makefile to create rpm from source code
- Minor additions to clitest to show 'fixed' behavior above.

* Mon Jan 11 2021 Rob Sanders <rsanders@forcepoint.com> 1.10.5
- Fix display issue when optional argument fails validation, so it will show 
  the invalid *value* rather than repeat the name

* Sun Jun 7 2020 Danial Zaoui <daniel.zaoui@yahoo.com>
- Fix function prototype in libcli.h to avoid error when compiled with 'strict-prototypes'

* Tue Mar 3 2020 Rob Sanders <rsanders@forcepoint.com> 1.10.4-1
- Fix segfault issue found during tab/help processing
- Minor fix of version on previous changelog record

* Fri Jan 10 2020 Rob Sanders <rsanders@forcepoint.com> 1.10.3-1
- Minor cosmetic change to how help messages are generated, minor edits
  to some comments, minor cosmetic change to clitest demo code

* Fri Dec 6 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.3-1
- Tweak to buildmode to only show optargs 'after' the point at
  which buildmode was entered.
- Add new 'cli_dump_optargs_and_args() function for development/debug
  Designed to be called from a callback to show output of optarg and
  argument processing.

* Thu Dec 5 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.3-1
- Updated CLI_CMD_OPTIONAL_FLAG parsing to use an validator function
  (if provided) to determine if the word being looked is a match for
  the optional flag.  If no validator function is provided then the 
  word much match the name of the optional flag exactly.

* Thu Nov 14 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.3-1
- Enhance how cli_parse_line handles quotes when parsine the command
  line.  This includs mixed single/double quotes, embedded quoted 
  substrings, and handling 'escaped' quotes using the '\' character.  
- Ensure that buildmode preserves 'empty' strings
  (ex: "", or '') when regenerating the cmdline after the user 'executes'
  the command.

* Fri Sep 7 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.2-1
- Fix bug where 'extra help' added with cli_optarg_addhelp() were not 
  being displayed for optional argumenets
- Fix bug if 'unset' called in buildmode w/o an argument
- Added completor/validator logic for buildmode 'unset' command
- Fixed bug in how help was being created in buildmode that resulted
  in badly formatted lines
- Add support so the buildmode unset command has dynamic help messages
  depending on what has already been set
- Prevent spot check optargs from appearing in buildmode
- BREAKING API CHANGE - the 'cli_register_optarg()' function is now returning
  a pointer to the newly added optarg or NULL instead of an integer 
  return code.  This is to fix a design bug where it was difficult to 
  use the new 'cli_optarg_addhelp()' function.  Only affects moving from
  1.10.0 to 1.10.2 (1.10.1 was never released)

* Tue Sep 3 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.2-1
- Fix bug in cli_optarg_addhelp()
- Change 'enter' to 'type' for buildmode commands autogenerated help
- Alter 'buildmode' help settings to indicate if a buildmode setting
  is optional by enclosing in '[]'

* Wed Aug 21 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.2-1
- Bug for processing an empty line, or empty command after a pipe character
- Bump version to 1.10.2-1

* Wed Aug 7 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.1-1
- Rework how help text is formatted to allow for word wrapping
  and embedded newlines
- Rework how 'buildmode' is show for help messages so it uses
  a single '*' as the first char of the help text.
- Rework optarg help text to allow the optarg to have specific
  help messages based on user input - look at clitest at the
  'shape' optarg examples.

* Wed Jul 30 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.1-1
- Bump version
- Renamed cli_int_[help|history|exit|quit|enable|disable] to
  same routine minut '_int_', and exposed in libcli.h.  Retained
  old command pointing to new command for backward compatability
- Fix coerner case in buildmode where no extra settings specified
  and user enters 'exit'
- Rename buildmode 'exit' command to 'execute' based on feedback

* Tue Jul 23 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.0-2
- Fix spec file and rpm build issues
- Fix 2 memory leaks (tab completion and help formatting)
- Expose cli_set_optarg_value() for external use

* Tue Jul 16 2019 Rob Sanders <rsanders@forcepoint.com> 1.10.0-1
- Add support for named arguments, optional flags, and optional arguments
- Support help and tab complete for options/arguments
- Enable users to add custom 'filters', including support for options/arguments
- Replaced current interal filters with equivalent 'user filters'
- Added examples of options/arguments to clitest
- Added support for 'building' longer commands one option/argument at a time (buildmode)
- Additional minor Coverity/valgrind related fixes
- Tab completion will show up until an ambiguity, or entire entry if only one found
- Tab completion for options/arguments will show '[]' around optional items
- Tab completion for options/arguments will show '<>' around required items
- Restructured clitest.c so 'cli_init()' is done in child thread

* Wed Sep 19 2018 Rob Sanders <rsanders@forcepoint.com> 1.9.8-4
- Update spac file to use relative links for libcli.so symlinks 

* Tue Sep 18 2018 Rob Sanders <rsanders@forcepoint.com> 1.9.8-3
- Update spec file similar to EPEL's for regular and devel pacakges
- Update Makefile rpm target to build both regular and devel pacakges
- Update changelog for new fixes)
- Update changelog (fix dates on several commits to avoid rpmbuild complaint)

* Sun Sep 16 2018 David Parrish <david@parrish.com> 1.9.8-2
- Reformat patches with clang-format

* Thu Sep 13 2018 Rob Sanders <rsanders@forcepoint.com> 1.9.8-1
- Fix segfaults processing long lines in cli_loop()
- Fix Coverity identified issues at the 'low' aggressive level

* Sun Jul 22 2012 David Parrish <david@dparrish.com> 1.9.7-1
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

* Sun Jul 27 2008 David Parrish <david@dparrish.com> 1.9.3-1
- Add support for compiling on WIN32 (Thanks Hamish Coleman)
- Fix cli_build_shortest() length handling
- Don't call cli_build_shortest() when registering every command
- Disable TAB completion during username entry

* Fri May  2 2008 David Parrish <david@dparrish.com> 1.9.2-1
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

* Sun Feb 18 2007 David Parrish <david@dparrish.com> 1.8.8-1
- Fix broken auth_callback logic - Thanks Ben Menchaca

* Thu Jun 22 2006 Brendan O'Dea <bod@optus.net> 1.8.7-1
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
