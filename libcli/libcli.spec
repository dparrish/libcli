Version: 1.0.0
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
find $RPM_BUILD_ROOT/usr -type f -print | grep -v '\/(README|COPYING|INSTALL|TODO|ChangeLog|AUTHORS|FAQ)$' | \
    sed "s@^$RPM_BUILD_ROOT@@g" | sed "s/^\(.*\)$/\1\*/" > %{name}-%{version}-filelist

%post
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}-%{version}-filelist
%defattr(-, root, root)

%doc README

%changelog

