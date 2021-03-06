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
#define PLAY 8

//Error constants
#define RESOURCES_ERROR 1
#define CONNECTION_ERROR 2

typedef struct Settings{
	int bandwidth;
	int remainingBandwidth;
	int mode;
	int rate;
	int resolution;
	char *ip;
	int messagePort;
	int videoPort;
	int audioPort;
	int ServerSocket;
} Settings;

void connect(Settings * settingsData);

int startStream(Settings * settingsData);

void stopStream(Settings * settingsData);

void pauseStream(Settings * settingsData);

void resumeStream(Settings * settingsData);

void rewindStream(Settings * settingsData);

void fastforwardStream(Settings * settingsData);

int calculateBandwidth(Settings * settingsData);

bool isEnoughBandwidth(Settings * settingsData);

int switchMode(Settings * settingsData);

int changeResources(Settings * settingsData);

void sendServerSignal(int signal);
