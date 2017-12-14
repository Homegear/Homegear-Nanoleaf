AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_nanoleaf.la
mod_nanoleaf_la_SOURCES = Nanoleaf.cpp Factory.cpp NanoleafPacket.cpp GD.cpp NanoleafPeer.cpp NanoleafCentral.cpp PacketManager.cpp
mod_nanoleaf_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_nanoleaf.la
