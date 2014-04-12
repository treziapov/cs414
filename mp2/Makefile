CC = g++
LINKER = g++
WARNINGS = -Wchar-subscripts -Wimplicit -Wparentheses -Wreturn-type -Wmissing-braces -Wundef -Wshadow
CC_OPTS = -Wall -Wextra 
GST = `pkg-config --cflags --libs gstreamer-0.10`

EXECS = streamer receiver

CODE_CLEAN = *.o $(EXECS)

all: $(EXECS)

streamer: server.cpp
	$(CC) server.cpp -o streamer $(GST)
	
receiver: client.cpp
	$(CC) client.cpp -o receiver $(GST)

clean:
	rm -f $(CODE_CLEAN)
	
	
