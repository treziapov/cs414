CC = g++
LINKER = g++
CC_OPTS = -Wall -Wextra -Wchar-subscripts -Wimplicit -Wparentheses -Wreturn-type -Wmissing-braces -Wundef -Wshadow
GST = `pkg-config --cflags --libs gstreamer-0.10`

EXECS = receiver
OBJS = connecter.o gst_client.o main.o

CODE_CLEAN = $(EXECS) $(OBJS)

all: $(EXECS)

connecter.o: connecter.cpp connecter.h
	$(CC) $(CC_OPTS) -c connecter.cpp

gst_client.o: gst_client.cpp gst_client.h
	$(CC) $(CC_OPTS) -c gst_client.cpp $(GST)

main.o : connecter.o gst_client.o main.cpp
	$(CC) $(CC_OPTS) -c main.cpp
	
receiver: connecter.o gst_client.o main.o
	$(LINKER) connecter.o gst_client.o main.o -o receiver $(GST)

clean:
	rm -f $(CODE_CLEAN)
	
	
