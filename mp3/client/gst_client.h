#include <gst/gst.h>

enum Mode {	Active = 1, Passive = 2 };


typedef struct SinkData{
	GstClockTime videoTSD;
	GstClockTime audioTSD;
	GstClockTime ping;
	int failures;
	int successes;
} SinkData;

class GstData {
public:
	int mode;
	gdouble playbackRate;
	gchar *serverIp, *clientIp;

	GstElement *pipeline;
	GstElement *videoUdpSource, *videoRtpDepay, *videoDecoder, *videoColorspace, *videoSink, *videoQueue;
	GstElement *audioUdpSource, *audioRtpDepay, *audioDecoder, *audioSink, *audioQueue, *audioVolume;
	GstCaps *videoUdpCaps, *audioUdpCaps, *videoDecCaps, *audioDecCaps;
	GstElement /**jitterBuffer,*/ *jitterTee, *jitterAppSink, *jitterQueue, *videoAppQueue;
	GstElement *videoTee, *videoAppSink, *videoDecAppQueue, *videoDecQueue;
	GstElement *audioTee, *audioAppSink, *audioAppQueue;
	GstElement *jitterBuffer, *tee, *appSink, *appQueue;

	GstData() {
		mode = Active;
		playbackRate = 1.0;
	};
};

class GstClient {
public:
	static const gint VIDEO_PORT = 5000;
	static const gint AUDIO_PORT = 5001;
	static const gchar* projectDirectory;

	static void initPipeline(GstData *data, int videoPort, int audioPort, SinkData * sinkData);
	static void buildPipeline(GstData *data);
	static void setPipelineToRun(GstData *data);
	static void waitForEosOrError(void *data);
	static void stopAndFreeResources(GstData *data);
	static void newBuffer(GstElement *sink, GstData *data);

	static void playPipeline(GstData *data);
	static void stopPipeline(GstData *data);
	static void pausePipeline(GstData *data);
	static void rewindPipeline(GstData *data);
	static void fastForwardPipeline(GstData *data);
	static void muteAudio(GstData *data);

	static char* getFilePathInHomeDirectory(const char* directory, const char* filename);
};

