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

//Error constants
#define RESOURCES_ERROR 1
#define CONNECTION_ERROR 2

typedef struct Settings{
	int bandwidth;
	int mode;
	int rate;
	int resolution;
	char * ip;
	int messagePort;
	int videoPort;
	int audioPort;
} Settings;

void connect(Settings * settingsData);

int startStream(Settings * settingsData);

void stopStream();

void pauseStream();

void resumeStream();

void rewindStream();

void fastforwardStream();

int calculateBandwidth(Settings * settingsData);

int switchMode(Settings * settingsData);

int changeResources(Settings * settingsData);