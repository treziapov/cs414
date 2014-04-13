#include <gst/gst.h>

enum Mode {	Active = 1, Passive = 2 };

class GstData {
	public:
		int mode;
		GstElement *pipeline;
		GstElement *videoUdpSource, *videoRtpDepay, *videoDecoder, *videoSink, *videoQueue;
		GstElement *audioUdpSource, *audioRtpDepay, *audioDecoder, *audioSink;
		GstCaps *videoUdpCaps, *audioUdpCaps;
		GstElement *jitterBuffer, *tee, *appSink, *appQueue;
};

class GstClient {
	public:
		static const gint VIDEO_PORT = 5000;
		static const gint AUDIO_PORT = 5001;

		static void initPipeline(GstData *data, int videoPort, int audioPort);
		static void buildPipeline(GstData *data);
		static void setPipelineToRun(GstData *data);
		static void waitForEosOrError(void *data);
		static void pausePipeline(GstData *data);
		static void stopPipeline(GstData *data);
		static void stopAndFreeResources(GstData *data);
		static void newBuffer(GstElement *sink, GstData *data);
};

