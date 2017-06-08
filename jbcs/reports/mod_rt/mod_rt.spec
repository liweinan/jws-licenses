Name: mod_rt
Summary: mod_rt is an apache httpd module
Version: 2.4.1.GA
Release: 12%{?dist}
License: GPL

# svn export http://svn.devel.redhat.com/repos/jboss-jon/tags/JBossON_2_4_1_GA/etc/product_connectors/apache-rt/sources/apache2.x/ mod_rt
# tar czf mod_rt-2.4.1.GA.tgz mod_rt
Source0: %{name}-%{version}.tgz
Source1: mod_rt.conf
Patch1: mod_rt-httpd.2.4.patch

Group: Development/Libraries

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

BuildRequires: libtool
BuildRequires: httpd-devel
BuildRequires: zip
Requires: httpd

#win-buildrequires: xbuild
#win-buildrequires: httpd
#sun-buildrequires: httpd

%description
This module implements End-user response time for Apache 2.0.

%prep
%setup -q -n %{name}-%{version}
%patch1 -p0 -b .httpd24

%build
make APXS=`which apxs`
                                                                                                     
%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_libdir}/httpd/modules \
         $RPM_BUILD_ROOT%{_sysconfdir}/httpd/conf.d

`which libtool` --mode=install cp mod_rt.la $RPM_BUILD_ROOT%{_libdir}/httpd/modules

chmod 755 $RPM_BUILD_ROOT%{_libdir}/httpd/modules/mod_rt.so
rm $RPM_BUILD_ROOT%{_libdir}/httpd/modules/mod_rt.a
rm $RPM_BUILD_ROOT%{_libdir}/httpd/modules/mod_rt.la

install -p -m 644 %{SOURCE1} \
        $RPM_BUILD_ROOT%{_sysconfdir}/httpd/conf.d/mod_rt.conf
                                                                                                    
%clean
rm -rf $RPM_BUILD_ROOT
                                                                                                     
%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/httpd/conf.d/mod_rt.conf
%{_libdir}/httpd/modules/*.so
#%{_libdir}/httpd/modules/*.a
#%{_libdir}/httpd/modules/*.la

%changelog
* Wed May 24 2017 Honza Fnukal <hfnukal@redhat.com> - 0:2.4.1.GA-12
- add pdb

* Mon Jul 13 2015 Mladen Turk <mturk@redhat.com> - 0:2.4.1.GA-8
- ER2 rebuild.

* Mon Feb 23 2015 Mladen Turk <mturk@redhat.com> - 0:2.4.1.GA-7
- Fis JWS-12

* Wed Jan 21 2015 Mladen Turk <mturk@redhat.com> - 0:2.4.1.GA-6
- Rebuild

* Wed Sep  5 2012 Mladen Turk <mturk@redhat.com> - 0:2.4.1.GA-3
- Rebuild

* Wed Apr 25 2012 Mladen Turk <mturk@redhat.com> - 0:2.4.1.GA-2
- Add support for windows and solaris builds

* Thu Mar 22 2012 Weinan Li <weli@redhat.com> - 0:2.4.1.GA-1
- Keep version in sync with JON

* Mon Mar 12 2012 Weinan Li <weli@redhat.com> - 0:2.0.0-2
- Move mod_rt.conf to git repo
- rename mod_rt.tgz
- remove mod_rt.a and mod_rt.la

* Mon Jan 09 2012 Weinan Li <weli@redhat.com> - 0:2.0.0-1
Initial import
