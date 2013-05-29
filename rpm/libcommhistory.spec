Name:       libcommhistory
Summary:    Communications event history database API
Version:    1.5.6
Release:    1
Group:      System/Libraries
License:    LGPL
URL:        https://github.com/nemomobile/libcommhistory
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.7.0
BuildRequires:  pkgconfig(QtContacts)
BuildRequires:  pkgconfig(QtSparql)
BuildRequires:  pkgconfig(tracker-sparql-0.14)
BuildRequires:  pkgconfig(QtDeclarative)
BuildRequires:  pkgconfig(QJson)
Requires: libqtsparql-tracker
Requires: libqtsparql-tracker-direct

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
%qmake 
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake_install

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/lib*.so*

%files tools
%defattr(-,root,root,-)
%{_bindir}/commhistory-tool

%files declarative
%defattr(-,root,root,-)
%{_libdir}/qt4/imports/org/nemomobile/commhistory/*

%files unit-tests
%defattr(-,root,root,-)
/opt/tests/libcommhistory-unit-tests/*

%files performance-tests
%defattr(-,root,root,-)
/opt/tests/libcommhistory-performance-tests/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libcommhistory.a
%{_libdir}/pkgconfig/commhistory.pc
%{_includedir}/commhistory/CommHistory/*

%files doc
%defattr(-,root,root,-)
%{_datadir}/doc/libcommhistory/*

