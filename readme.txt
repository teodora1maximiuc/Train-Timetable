Mersul trenurilor

compilare server: gcc -g server.c -o sv -lsqlite3 -I/usr/include/libxml2 -lxml2
compilare client: gcc interfata.c -o int `pkg-config --cflags --libs gtk+-3.0`
