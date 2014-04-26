CC = g++
LINKER = g++
CC_OPTS = -Wall -Wextra -Wchar-subscripts -Wimplicit -Wparentheses -Wreturn-type -Wmissing-braces -Wundef -Wshadow
GST = `pkg-config --cflags --libs gstreamer-0.10`

EXECS = streamer
OBJS = listener.o gst_server.o

CODE_CLEAN = $(EXECS) $(OBJS)

all: $(EXECS)

listener.o: listener.cpp listener.h
	$(CC) $(CC_OPTS) -c listener.cpp $(GST)

gst_server.o: gst_server.cpp gst_server.h
	$(CC) $(CC_OPTS) -c gst_server.cpp $(GST)

main.o: listener.o gst_server.o main.cpp
	$(CC) $(CC_OPTS) -c main.cpp $(GST)
	
streamer: listener.o gst_server.o main.o
	$(LINKER) connecter.o gst_server.o main.o -o streamer $(GST)

clean:
	rm -f $(CODE_CLEAN)
	
	
