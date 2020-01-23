
CC = gcc

CFLAGS += -DHAVE_CONFIG_H -I../src -I../lib -I../plugin -I. -I.. -Icrypt -lm -Wall -Wno-unused -fomit-frame-pointer

GIFT_INSTALL_DIR=$(HOME)/.giFT
FST_INSTALL_DIR=$(GIFT_INSTALL_DIR)/FastTrack

#DEBUGFLAGS = -D DEBUG -D HEAVY_DEBUG
#DEBUGFLAGS = -D DEBUG
DEBUGFLAGS = 

FST_SOURCES = fst_fasttrack.c fst_download.c fst_hash.c fst_http.c fst_meta.c fst_node.c fst_packet.c fst_search.c fst_session.c fst_stats.c fst_utils.c md5.c
CRYPT_SOURCES = crypt/fst_crypt.c crypt/enc_type_1.c crypt/enc_type_2.c crypt/enc_type_20.c


all:
	@echo "*************************************************************"
	@echo "Ok, lets see if we can get this put together."
	@echo "*************************************************************"
	@echo
	$(CC) -shared -o FastTrack.so $(CFLAGS) $(DEBUGFLAGS) $(FST_SOURCES) $(CRYPT_SOURCES)
#	$(CC) -dynamiclib -flat_namespace -undefined suppress -o FastTrack.so $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) $(FST_SOURCES) $(CRYPT_SOURCES)
	@echo
	@echo "*************************************************************"
	@echo Your giFT settings path is $(GIFT_INSTALL_DIR) and the plugin will be installed into $(FST_INSTALL_DIR).
	@echo "If this is correct you can run 'make install' now."
	@echo "If giFT is NOT installed in the above place you MUST change the path in the Makefile before running 'make install'"
	@echo "*************************************************************"

install:
	@echo "*************************************************************"
	@echo "Copying files..."
	@echo "*************************************************************"
	@echo
	@echo Creating dir $(FST_INSTALL_DIR) if it doesn\'t already exist
	@mkdir -p $(FST_INSTALL_DIR)
	@echo moving ./FastTrack.so to /usr/local/lib/giFT/FastTrack.so
	@mv ./FastTrack.so /usr/local/lib/giFT/FastTrack.so
	@echo copying ./data/nodes to $(FST_INSTALL_DIR)/nodes
	@cp ./data/nodes $(FST_INSTALL_DIR)/nodes
	@echo
	@echo "*************************************************************"
	@echo "Ok we're done."
	@echo "Start giFT with 'giFT -p /usr/local/lib/giFT/FastTrack.so' to give it a try."
	@echo "Alternatively you can add '/usr/local/lib/giFT/FastTrack.so' to gift.conf"
	@echo "*************************************************************"
	 	
