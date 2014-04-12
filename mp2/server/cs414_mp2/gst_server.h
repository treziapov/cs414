#include <gst/gst.h>

enum Mode { Active = 1, Passive = 2 };

class GstData {
	public:
		Mode mode;
		gchar* clientIp;
		GstElement *pipeline;
		GstElement *videoSource, *videoEncoder, *videoRtpPay, *videoUdpSink;
		GstElement *audioSource, *audioEncoder, *audioRtpPay, *audioUdpSink;

		~GstData() {
			delete clientIp;
		};
};

class GstServer {
	public:
		static const gint VIDEO_PORT = 5000;
		static const gint AUDIO_PORT = 5001;

		static void initPipeline(GstData *data, int videoPort, int audioPort);
		static void buildPipeline(GstData *data);
		static void setPipelineToRun(GstData *data);
		static void waitForEosOrError(GstData *data);
		static void stopAndFreeResources(GstData *data);
};