#include <gst/gst.h>

enum Mode { Active = 1, Passive = 2 };
enum Resolution { R240 = 1, R480 = 2 };

class GstData {
	public:
		Mode mode; 
		Resolution resolution;

		gchar* clientIp;
		gint videoPort, audioPort, videoFrameRate;
		gdouble playbackRate;

		GstElement *pipeline, *fileSource, *decodeBin, *decoder;
		GstElement *videoDecoder;
		GstElement *videoQueue, *videoRate, *videoCapsFilter, *videoEncoder, *videoRtpPay, *videoUdpSink;
		GstElement *audioQueue, *audioRate, *audioCapsFilter, *audioEncoder, *audioRtpPay, *audioUdpSink;

		GstData() {
			mode = Active;
			playbackRate = 1.0;
			clientIp = (gchar *)"localhost";
		};

		~GstData() {
			delete clientIp;
		};
};

class GstServer {
	public:
		static const gchar* videoName480p;
		static const gchar* videoName240p;
		static const gchar* videoDirectory;

		static void initPipeline(GstData *data);
		static void configurePipeline(GstData *data);
		static void buildPipeline(GstData *data);
		static void setPipelineToRun(GstData *data);
		static void stopAndFreeResources(GstData *data);

		static void * waitForEosOrError(void *data);

		static void playPipeline(GstData *data);
		static void stopPipeline(GstData *data);
		static void pausePipeline(GstData *data);
		static void rewindPipeline(GstData *data);
		static void fastForwardPipeline(GstData *data);

		static char* getFilePathInHomeDirectory(const char* directory, const char* filename);
};
