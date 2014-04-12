#include <gst/gst.h>

enum Mode { Active = 1, Passive = 2 };

class GstData {
	public:
		Mode mode;
		gchar* clientIp;
		GstElement *pipeline;
		GstElement *videoSource, *videoEncoder, *videoRtpPay, *videoUdpSink;
		GstElement *audioSource, *audioEncoder, *audioRtpPay, *audioUdpSink;

		GstData() {
			mode = Active;
			clientIp = "localhost";
		};

		~GstData() {
			delete clientIp;
		};
};

class GstServer {
	public:
		static const gint VIDEO_PORT = 5000;
		static const gint AUDIO_PORT = 5001;

		static void initPipeline(GstData *data, int videoPort, int audioPort);
		static void buildPipeline(GstData *data, int streamMode);
		static void setPipelineToRun(GstData *data);
		static void waitForEosOrError(void *data);
		static void stopAndFreeResources(GstData *data);

		static void playPipeline(GstData *data);
		static void stopPipeline(GstData *data);
		static void pausePipeline(GstData *data);
};