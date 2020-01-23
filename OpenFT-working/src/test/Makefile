GIFT=../../../giFT
db_bench: db_bench.c ../ft_search_db.c
	gcc -g -Wall -DHAVE_CONFIG_H -DBENCHMARK db_bench.c -I/usr/local/lib/BerkeleyDB.4.1/include -I. -I.. -I../.. -I../../lib -I../../src -I../../plugin ../md5.o ../ft_share_file.o $(GIFT)/src/meta.o $(GIFT)/src/meta/meta*.c ../ft_conf.c $(GIFT)/src/mime.o  -logg -lvorbis -lvorbisfile -L/usr/local/BerkeleyDB.4.1/lib -ldb-4.1 -lgift -lgiftproto -I$(GIFT) -I$(GIFT)/src  -o db_bench 

queries: ~/.giFT/gift.log
	perl -pe '$$_=/ft_search_request.*'\''(.*)'\''/ && $$1.$$/' ~/.giFT/gift.log >queries
