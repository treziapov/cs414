//#include <vector>
#include <WinSock2.h>

#include "gst_server.h"

//Resolution constants
#define R240 1
#define R480 2

//Mode constants
#define ACTIVE 1
#define PASSIVE 2

//Stream accept/reject
#define ACCEPT 1
#define REJECT 2

//Stream function constants
#define STOP 1
#define PAUSE 2
#define RESUME 3
#define REWIND 4
#define FAST_FORWARD 5
#define SWITCH_MODE 6
#define NEW_RESOURCES 7

typedef struct Client{
	int bandwidth;
	int port;
	GstData gstData;
} Client;

typedef struct Resources{
	int totalBandwidth;
	int remainingBandwidth;
	Client ** clients;
	int numClients;
} Resources;

typedef struct Request{
	int resolution;
	int mode;
	int rate;
} Request;

typedef struct ThreadData{
	int port;
	int videoPort;
	int audioPort;
	int resolution;
	int mode;
	int rate;
	SOCKET ClientSocket;
	GstData *gstData;
} ThreadData;

void init_listener(int totalBandwidth);
int calculateResources(Resources rsc, Request req);
Client * createClient(Resources & resource, int clientBandwidth, int clientPort);
void handleConnection(void * ptr);