db_bench: db_bench.c ../ft_search_db.c
	gcc -g -Wall -DHAVE_CONFIG_H -DBENCHMARK db_bench.c -I. -I.. -I../.. -I../../lib -I../../src -I../../plugin ../md5.o ../ft_share_file.o ../../src/meta*.o ../../src/mime.o  ../ft_shost.c -logg -lvorbis -lvorbisfile -ldb -lgift -lgiftproto -o db_bench 

queries: ~/.giFT/gift.log
	perl -pe '$$_=/ft_search_request.*'\''(.*)'\''/ && $$1.$$/' ~/.giFT/gift.log >queries
