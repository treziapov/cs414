#include <gst/gst.h>
#include <iostream>
#include <string>

#define MJPEG "jpegenc"
#define MPEG4 "ffenc_mpeg4"
#define AVI_MUXER "avimux"
#define MKV_MUXER "matroskamux"
#define AVI_DEMUXER "avidemux"
#define MKV_DEMUXER "matroskademux"

using namespace std;

enum PlayerMode { Camera, File };

typedef struct _VideoSettings 
{
	string source;
	int height;
	int width;
	int rate;
	string encoder;
	string decoder;
	string muxer;
	string demuxer;
	string fileSourcePath;
	string fileSinkPath;
} VideoSettings;

/*
	Other pipelines:

	"ksvideosrc ! video/x-raw-yuv,width=640,height=480 ! ffmpegcolorspace ! tee name=my_videosink ! jpegenc ! avimux ! filesink location=video.avi my_videosink. ! queue ! autovideosink"

	"ksvideosrc ! video/x-raw-yuv,width=640,height=480 ! ffmpegcolorspace ! autovideosink"

	"filesrc location="C:\\Users\\Timur\\Documents\\Visual Studio 2010\\Projects\\cs414\\cs414\\test.avi" ! decodebin2 ! ffmpegcolorspace ! autovideosink"
*/
int main(int argc, char *argv[]) 
{
	GstElement *pipeline, *muxer, *demuxer, *tee, *fileQueue, *playerQueue, *fileSource, *fileSink;
	GstElement *videoSource, *videoSink, *videoFilter, *videoConvert, *videoEncoder, *videoDecoder;
	GstCaps *videoCaps;
	GstBus *bus;
	GstMessage *msg;

	/* Set default video player settings */
	PlayerMode mode = File;
	VideoSettings videoSettings;
	videoSettings.height = 480;
	videoSettings.width = 640;
	videoSettings.rate = 20;
	videoSettings.encoder = MJPEG;
	videoSettings.decoder = MJPEG;
	videoSettings.muxer = AVI_MUXER;
	videoSettings.demuxer = AVI_MUXER;
	videoSettings.fileSourcePath = "test.avi";
	videoSettings.fileSinkPath = "video" + string(videoSettings.muxer == AVI_MUXER ? ".avi" : ".mp4");		// TODO: add correct extension

	/* Initialize GStreamer */
	gst_init(&argc, &argv);
  
	/* Build Elements */
	pipeline = gst_pipeline_new("mp1-pipeline");
	tee = gst_element_factory_make("tee", "tee1");
	fileQueue = gst_element_factory_make("queue", "fileQueue");
	playerQueue = gst_element_factory_make("queue", "playerQueue");
	fileSource = gst_element_factory_make("filesrc", "fileSource");
	fileSink = gst_element_factory_make("filesink", "filesink");

	muxer = gst_element_factory_make(videoSettings.muxer.c_str(), "muxer");
	if (muxer == NULL) g_error("Could not create muxer element");
	demuxer = gst_element_factory_make(videoSettings.demuxer.c_str(), "muxer");
	if (demuxer == NULL) g_error("Could not create demuxer element");
	videoSource = gst_element_factory_make("dshowvideosrc", "videoSource");
	if (videoSource == NULL) g_error("Could not create video source element");
	videoSink = gst_element_factory_make("dshowvideosink", "videoSink");
	if (videoSink == NULL) g_error("Could not create video sink element");
	videoConvert = gst_element_factory_make("autovideoconvert", "videoConvert");
	if (videoConvert == NULL) g_error("Could not create video convert element");
	videoFilter = gst_element_factory_make("capsfilter", "videoFilter");
	if (videoFilter == NULL) g_error("Could not create video filter element");
	videoEncoder = gst_element_factory_make(videoSettings.encoder.c_str(), "videoEncoder");
	if (videoEncoder == NULL) g_error("Could not create video encoder element");
	videoDecoder = gst_element_factory_make(videoSettings.decoder.c_str(), "videoDecoder");
	if (videoDecoder == NULL) g_error("Could not create video decoder element");

	/* Set values */
	g_object_set(G_OBJECT(fileSink), "location", videoSettings.fileSinkPath.c_str(), NULL);
	g_object_set(G_OBJECT(fileSource), "location", videoSettings.fileSourcePath.c_str(), NULL);

	videoCaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
		"height", G_TYPE_INT, videoSettings.height,
		"width", G_TYPE_INT, videoSettings.width,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"framerate", GST_TYPE_FRACTION, videoSettings.rate, 1, 
		NULL);
	g_object_set(G_OBJECT(videoFilter), "caps", videoCaps, NULL);
	gst_caps_unref (videoCaps);

	/* Build the pipeline */
	gst_bin_add_many(GST_BIN(pipeline), videoSource, videoConvert, videoFilter, tee, fileQueue, videoEncoder, muxer, fileSink, playerQueue, videoSink, NULL);
	
	switch (mode)
	{
		case Camera:
			/* Link the video recording component: videoSrc -> convert -> filter -> tee */
			if (!gst_element_link_many(videoSource, videoConvert, videoFilter, tee, NULL))
			{
				gst_object_unref(pipeline);
				g_error("Failed linking: videoSrc -> convert -> filter -> tee \n");
			}
			/* Link the media sink pipeline: tee -> queue -> encoder -> muxer -> fileSink */
			if (!gst_element_link_many(tee, fileQueue, videoEncoder, muxer, fileSink, NULL)) 
			{
				gst_object_unref(pipeline);
				g_print("Failed linking: tee -> queue -> encoder -> muxer -> fileSink \n");
			}
			/* Link the media player pipeline: tee -> queue -> mediaSink */
			if (!gst_element_link_many(tee, playerQueue, videoSink, NULL)) 
			{
				gst_object_unref(pipeline);
				g_print("Failed linking: tee -> queue -> mediaSink \n");
			}
			break;

		case File:
			/* Link the source file to video sink component: fileSrc -> demuxer -> decoder -> mediaSink */
			if (!gst_element_link_many(fileSource, demuxer, videoDecoder, videoSink, NULL))
			{
				gst_object_unref(pipeline);
				g_error("Failed linking: fileSrc -> demuxer -> decoder -> mediaSinke \n");
			}
			break;
	}

	/* Run */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
  
	/* Wait until error or EOS */
	bus = gst_element_get_bus(pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
	
	/* Parse message */
	if (msg != NULL) {
		GError *err;
		gchar *debug_info;
     
		switch (GST_MESSAGE_TYPE (msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error (msg, &err, &debug_info);
				g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
				g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
				g_clear_error (&err);
				g_free (debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print ("End-Of-Stream reached.\n");
				break;
			default:
				/* We should not reach here because we only asked for ERRORs and EOS */
				g_printerr ("Unexpected message received.\n");
				break;
			}
		gst_message_unref (msg);
	}
    
	gst_object_unref (bus);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (pipeline);
	return 0;
}