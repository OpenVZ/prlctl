all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean

install:
	$(MAKE) -C src install
	install -d $(DESTDIR)/usr/share/man/man8
	install -m 644 man/prlctl.8 $(DESTDIR)/usr/share/man/man8
	install -m 644 man/prlsrvctl.8 $(DESTDIR)/usr/share/man/man8
	install -d $(DESTDIR)/etc/bash_completion.d
	install -m 755 bash_completion/prlctl.sh $(DESTDIR)/etc/bash_completion.d/prlctl.sh
	install -d $(DESTDIR)/usr/share/pmigrate
	install -m 755 scripts/pmigrate_local_login.py $(DESTDIR)/usr/share/pmigrate/pmigrate_local_login.py

depend:
	$(MAKE) -C src depend

.PHONY: all install clean depend
