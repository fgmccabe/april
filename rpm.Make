# Generate RPM 
# -*- Makefile -*-
#

rpm:	$(distdir).tar.gz
	rm -rf rpm_build
	mkdir rpm_build/{,SPECS,BUILD,SRPMS,RPMS,SOURCES}
	rpm --define "_topdir $(PWD)/rpm_build" -ta $<
	-mv `find rpm_build/RPMS -name '*.rpm'` .
	-mv rpm_build/SRPMS/* .
	rm -rf rpm_build

