CFLAGS = -I/usr/local/opt/openssl/include
CFLAGS += -std=c99
CFLAGS += $$(pkg-config vips --cflags)

LDLIBS = $$(pkg-config vips --libs) -lm
LDLIBS += -lcrypto -lm
LDLIBS += -ljson-c
LDLIBS += -lmongoose

LDFLAGS = -L libmongoose

all: pictDBM pictDB_server

error.o: error.c error.h
pictDBM.o: pictDBM.c pictDB.h
image_content.o: image_content.c image_content.h
pictDBM_tools.o: pictDBM_tools.c pictDBM_tools.h
dedup.o: dedup.c dedup.h
db_utils.o : db_utils.c
db_list.o : db_list.c
db_create.o : db_create.c
db_delete.o : db_delete.c
db_insert.o : db_insert.c
db_read.o : db_read.c
db_gbcollect.o : db_gbcollect.c

pictDBM: error.o pictDBM.o image_content.o pictDBM_tools.o dedup.o db_utils.o db_list.o db_create.o db_delete.o db_insert.o db_read.o db_gbcollect.o

pictDB_server: error.o pictDB_server.c db_list.o pictDB.h db_utils.o db_read.o image_content.o db_insert.o db_delete.o dedup.o

clean:
	rm *.o
