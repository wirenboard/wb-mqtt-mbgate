install:
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas

	install -m 0644 wb-mqtt-mbgate.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-mbgate.schema.json
