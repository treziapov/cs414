#include <gst/gst.h>
#include <gtk-2.0\gtk\gtk.h>
#include <string>
#include "utility.h"

/* GStreamer Plugin Definitions */
#define MJPEG_ENCODER "jpegenc"
#define MJPEG_DECODER "jpegdec"
#define MPEG4_ENCODER "ffenc_mpeg4"
#define MPEG4_DECODER "ffdec_mpeg4"
#define AVI_MUXER "avimux"
#define AVI_DEMUXER "avidemux"
#define MKV_MUXER "matroskamux"
#define MKV_DEMUXER "matroskademux"
#define IDENTITY "identity"
#define TEST_VIDEO_SOURCE "C:/Users/Timur/Documents/Visual Studio 2010/Projects/cs414/cs414/test.avi"

using namespace std;

enum PlayerMode { Initial, Camera, File };

typedef struct _VideoData 
{
	GstElement *pipeline, *muxer, *demuxer, *tee, *fileQueue, *playerQueue, *fileSource, *fileSink;
	GstElement *videoSource, *videoSink, *videoFilter, *videoConvert, *videoEncoder, *videoDecoder;
	GstCaps *videoCaps;
	GstState state;
	gint64 duration;

	GtkWidget *slider;
	gulong sliderUpdateSignalId;

	int height, width, recordingRate;
	double playbackRate;
	bool playing;

	string encoderPlugin, decoderPlugin;
	string muxerPlugin, demuxerPlugin;
	string fileSourcePath, fileSinkPath;

	PlayerMode playerMode;
} VideoData;

void sendSeekEvent(VideoData *data);
void gstreamerSetup(int argc, char *argv[], VideoData *videoData);
void gstreamerPlay(VideoData *videoData);
void gstreamerPause(VideoData *videoData);
void gstreamerCleanup(VideoData *videoData);
void gstreamerBuildPipeline(VideoData *videoData, PlayerMode mode);
void gstreamerBuildDynamicElements(VideoData *videoData);
void initializeVideoData(VideoData * videoData);

void gstreamerPlayVideoFile(GtkWidget * widget, VideoData * videoData);
void gstreamerPauseVideoFile(GtkWidget * widget, VideoData * videoData);
void gstreamerStartCameraVideoCapture(GtkWidget *widget, VideoData *videoData);
void gstreamerStopCameraVideoCapture(GtkWidget *widget, VideoData *videoData);
	