EXTRA_DIST = doxygen.conf.in

if DOXYGEN_DOC
noinst_DATA = html
html: doxygen.conf
	$(DOXYGEN) doxygen.conf

install-data-local:
	$(mkdir_p) $(DESTDIR)/$(docdir)
	chmod u+w $(DESTDIR)/$(docdir)
	cp -r $(builddir)/html $(DESTDIR)/$(docdir)
uninstall-local:
	rm -rf $(DESTDIR)/$(docdir)/html

clean-local:
	rm -f *~
	rm -rf html latex

endif