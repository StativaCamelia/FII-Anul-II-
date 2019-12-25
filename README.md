gcc server.c -pthread -l sqlite3  -w -o server $(xml2-config --cflags) $(xml2-config --libs)
