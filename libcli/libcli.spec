Version: 1.8.0
Summary: Cisco-like telnet command-line library
Name: libcli
Release: 1
Copyright: LGPL
Group: Library/Communication
Source: %{name}-%{version}.tar.gz
URL: http://www.sf.net/projects/libcli
Packager: David Parrish <david@dparrish.com>
BuildRoot: /var/tmp/%{name}-buildroot/

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
make PREFIX=$RPM_BUILD_ROOT/usr install
find $RPM_BUILD_ROOT/usr ! -type d -print | grep -v '\/(README|\.html)$' | \
    sed "s@^$RPM_BUILD_ROOT@@g" | sed "s/^\(.*\)$/\1\*/" > %{name}-%{version}-filelist

%post
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}-%{version}-filelist
%defattr(-, root, root)

%doc README Doc/usersguide.html Doc/developers.html

%changelog
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
