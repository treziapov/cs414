#include <gst/gst.h>

enum Mode { Active = 1, Passive = 2 };

class GstData {
	public:
		Mode mode; 
		gchar* clientIp;
		gint videoPort, audioPort;

		GstElement *pipeline, *fileSource, *decoder;
		GstElement *videoQueue, *videoRate, *videoCapsFilter, *videoEncoder, *videoRtpPay, *videoUdpSink;
		GstElement *audioQueue,*audioEncoder, *audioRtpPay, *audioUdpSink;

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
		static const gint DEFAULT_VIDEO_PORT = 5000;
		static const gint DEFAULT_AUDIO_PORT = 5001;
		static const gchar* videoName480p;
		static const gchar* videoName240p;
		static const gchar* videoDirectory;

		static void initPipeline(GstData *data);
		static void configurePipeline(GstData *data);
		static void buildPipeline(GstData *data);
		static void setPipelineToRun(GstData *data);
		static void stopAndFreeResources(GstData *data);

		static void waitForEosOrError(void *data);

		static void playPipeline(GstData *data);
		static void stopPipeline(GstData *data);
		static void pausePipeline(GstData *data);

		static char* getFilePathInHomeDirectory(const char* directory, const char* filename);
};