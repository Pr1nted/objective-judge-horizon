Name:           ojh
Version:        {{version}}
Release:        1%{?dist}
Summary:        {{summary}}
License:        {{license}}
URL:            {{homepage}}
Source0:        {{url_source}}
BuildRequires:  cmake
BuildRequires:  gcc

%description
{{description}}

%prep
%autosetup -n ojh-%{version}

%build
%cmake
%cmake_build

%check
%ctest

%install
%cmake_install

%files
%{_bindir}/ojh
%{_datadir}/ojh/
%{_datadir}/doc/ojh/
