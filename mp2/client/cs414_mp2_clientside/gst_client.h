#include <gst/gst.h>

enum Mode {	Active = 0, Passive = 1 };

class GstData {
	public:
		int mode;
		GstElement *pipeline;
		GstElement *videoUdpSource, *videoRtpDepay, *videoDecoder, *videoSink;
		GstElement *audioUdpSource, *audioRtpDepay, *audioDecoder, *audioSink;
		GstCaps *videoUdpCaps, *audioUdpCaps;
};

class GstClient {
	public:
		static const gint VIDEO_PORT = 5000;
		static const gint AUDIO_PORT = 5001;

		static void initPipeline(GstData *data);
		static void buildPipeline(GstData *data);
		static void setPipelineToRun(GstData *data);
		static void waitForEosOrError(GstData *data);
		static void pausePipeline(GstData *data);
		static void stopPipeline(GstData *data);
		static void stopAndFreeResources(GstData *data);
};
