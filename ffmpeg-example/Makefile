CFLAGS=-I/usr/include -L/usr/lib -L/usr/local/lib -I/usr/include/libdrm
LIBS=-lavformat -lavcodec -lswscale -lavutil -ldl -ldrm -lswresample
LDFLAGS="-L/usr/lib -L/usr/local/lib"
C_SRC = $(wildcard *.c)
C_OBJ = $(patsubst %c, %o, $(C_SRC))
.PHONY:all clean
all: ffmpegexample
%o: %c
	$(CC) $(CFLAGS) -c $< 
ffmpegexample:$(C_OBJ)
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) 
clean:
	rm *.o -f
