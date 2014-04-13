#include <gst/gst.h>

enum Mode {	Active = 1, Passive = 2 };


typedef struct SinkData{
	GstClockTime videoTSD;
	GstClockTime audioTSD;
	GstClockTime ping;
	int failures;
} SinkData;

class GstData {
public:
	int mode;
	double playbackRate;
	GstElement *pipeline;
	GstElement *videoUdpSource, *videoRtpDepay, *videoDecoder, *videoSink, *videoQueue;
	GstElement *audioUdpSource, *audioRtpDepay, *audioDecoder, *audioSink, *audioQueue;
	GstCaps *videoUdpCaps, *audioUdpCaps;
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

	static void initPipeline(GstData *data, int videoPort, int audioPort);
	static void buildPipeline(GstData *data);
	static void setPipelineToRun(GstData *data);
	static void waitForEosOrError(void *data);
	static void stopAndFreeResources(GstData *data);
	static void newBuffer(GstElement *sink, GstData *data);

	static void playPipeline(GstData *data);
	static void stopPipeline(GstData *data);
	static void pausePipeline(GstData *data);
	static void rewindVideo(GstData *data);
	static void fastForwardVideo(GstData *data);
};

