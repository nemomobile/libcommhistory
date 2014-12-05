Name:       libcommhistory-qt5
Summary:    Communications event history database API
Version:    1.7.22
Release:    1
Group:      System/Libraries
License:    LGPL
URL:        https://github.com/nemomobile/libcommhistory
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.1.41
BuildRequires:  pkgconfig(contactcache-qt5) >= 0.0.17

%description
Library for accessing the communications (IM, SMS and call) history database.

%package unit-tests
Summary: Unit Test files for libcommhistory
Group: Development/Libraries

%description unit-tests
Unit Test files for libcommhistory

%package performance-tests
Summary: Performance Test files for libcommhistory
Group: Development/Libraries

%description performance-tests
Performance Test files for libcommhistory

%package tools
Summary: Command line tools for libcommhistory
Group: Communications/Telephony and IM
Requires: %{name} = %{version}-%{release}
Conflicts: libcommhistory-tools

%description tools
Command line tools for the commhistory library.

%package declarative
Summary: QML plugin for libcommhistory
Group: System/Libraries
Requires: %{name} = %{version}-%{release}

%description declarative
QML plugin for libcommhistory

%package devel
Summary: Development files for libcommhistory
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Headers and static libraries for the commhistory library.


%package doc
Summary: Documentation for libcommhistory
Group: Documentation

%description doc
Documentation for libcommhistory


%prep
%setup -q -n %{name}-%{version}

%build
unset LD_AS_NEEDED
%qmake5
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake_install

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%post unit-tests
for n in ut_recentcontactsmodel ut_callmodel; do
    pathname=/opt/tests/libcommhistory-qt5-unit-tests/$n
    chgrp privileged $pathname && chmod g+s $pathname
done

%files
%defattr(-,root,root,-)
%{_libdir}/libcommhistory-qt5.so*

%files tools
%defattr(-,root,root,-)
%{_bindir}/commhistory-tool

%files declarative
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/org/nemomobile/commhistory/*

%files unit-tests
%defattr(-,root,root,-)
/opt/tests/libcommhistory-qt5-unit-tests/*

%files performance-tests
%defattr(-,root,root,-)
/opt/tests/libcommhistory-qt5-performance-tests/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/commhistory-qt5.pc
%{_includedir}/commhistory-qt5/CommHistory/*

%files doc
%defattr(-,root,root,-)
%{_datadir}/doc/libcommhistory-qt5/*

