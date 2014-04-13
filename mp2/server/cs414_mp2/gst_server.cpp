#include <process.h>
#include <Windows.h>

#include "gst_server.h"

const gchar* GstServer::videoDirectory = "/Documents/GitHub/cs414/mp2/server/cs414_mp2/";
const gchar* GstServer::videoName480p = "mjpeg_640x480.avi";
const gchar* GstServer::videoName240p = "mjpeg_320x240.avi";

char* GstServer::getFilePathInHomeDirectory(const char* directory, const char* filename) {
	char* home = getenv("HOMEPATH");
	if (!home) {
		home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "Couldn't find HOMEPATH or HOME environment variable\n");
			return NULL;
		}
	}
	char *curr = home;
	while (*curr != 0) {
	if (*curr == '\\')
		*curr = '/';
		curr;
	}
	
	char path[124];
	strcpy (path, home);
	strcat (path, directory);
	strcat (path, filename);
	printf ("getFilePathInHomeDirectory: generated path - %s\n", path);
	return path;
}

void GstServer::initPipeline(GstData *data) {
	gst_init (NULL, NULL);

	data->pipeline = gst_pipeline_new ("streaming_server_pipeline");
	data->fileSource = gst_element_factory_make ("filesrc", "fileSource");
	data->decoder = gst_element_factory_make ("decodebin2", "decoder");

	data->videoQueue = gst_element_factory_make ("queue", "videoQueue");
	data->videoRate = gst_element_factory_make ("videorate", "videoRate");
	data->videoCapsFilter = gst_element_factory_make ("capsfilter", "videoCapsFilter");
	data->videoEncoder = gst_element_factory_make ("jpegenc", "videoEncoder");
	data->videoRtpPay = gst_element_factory_make ("rtpjpegpay", "videoRtpPay");
	data->videoUdpSink = gst_element_factory_make ("udpsink", "videoUdpSink");

	data->audioQueue = gst_element_factory_make ("queue", "audioQueue");
	data->audioEncoder = gst_element_factory_make ("mulawenc", "audioEncoder");
	data->audioRtpPay = gst_element_factory_make ("rtppcmupay", "audioRtpPay");
	data->audioUdpSink = gst_element_factory_make ("udpsink", "audioUdpSink");

	if (!data->pipeline || !data->decoder || !data->fileSource ) {
		g_printerr ("Not all general pipeline elements could be created.\n");
	}

	if ( !data->videoQueue || !data->videoCapsFilter || !data->videoRate || !data->videoEncoder || 
		!data->videoRtpPay || !data->videoUdpSink) {
			g_printerr ("Not all video elements could be created.\n");
	}
	if (!data->audioQueue || !data->audioEncoder || !data->audioRtpPay || !data->audioUdpSink) {
			g_printerr ("Not all audio elements could be created.\n");
	}
}

void GstServer::configurePipeline(GstData *data) {
	gchar *videoFilePath;
	gint videoFrameRate;
	if (data->mode == Active) {
		videoFrameRate = 20;
		videoFilePath = getFilePathInHomeDirectory (videoDirectory, videoName480p);
	}
	else {
		videoFrameRate = 10;
		videoFilePath = getFilePathInHomeDirectory (videoDirectory, videoName240p);
	}

	GstCaps *videoRateCaps = gst_caps_new_simple (
		"video/x-raw-yuv",
		"framerate", GST_TYPE_FRACTION, videoFrameRate, 1, NULL);

	g_object_set (data->videoCapsFilter, "caps", videoRateCaps, NULL);
	g_object_set (data->fileSource, "location", videoFilePath, NULL);
	g_object_set (data->videoUdpSink, "host", data->clientIp, "port", data->videoPort, NULL);
	g_object_set (data->audioUdpSink, "host", data->clientIp, "port", data->audioPort, NULL);
}

void GstServer::buildPipeline(GstData *data) {
	if(data->mode == Active){
		gst_bin_add_many (
			GST_BIN (data->pipeline), 
			data->fileSource, data->decoder,
			data->videoQueue, data->videoRate, data->videoCapsFilter, 
			data->videoEncoder, data->videoRtpPay, data->videoUdpSink,
			data->audioQueue, data->audioEncoder, data->audioRtpPay, data->audioUdpSink, NULL);
		if (!gst_element_link(data->fileSource, data->decoder)) {
			g_printerr ("Failed to link fileSource with decoder\n");
		}
		if (!gst_element_link_many(data->decoder, data->videoQueue, data->videoRate, 
				data->videoCapsFilter, data->videoEncoder, data->videoRtpPay, data->videoUdpSink, NULL)) {
					g_printerr ("Failed to link video streaming pipeline\n");
		}
		if (!gst_element_link_many(data->decoder, data->audioQueue, data->audioEncoder, 
				data->audioRtpPay, data->audioUdpSink, NULL)) {
					g_printerr ("Failed to link audio streaming pipeline\n");
		}
	}else{
		gst_bin_add_many (
			GST_BIN (data->pipeline), 
			data->fileSource, data->decoder,
			data->videoQueue, data->videoRate, data->videoCapsFilter, 
			data->videoEncoder, data->videoRtpPay, data->videoUdpSink, NULL);
		if (!gst_element_link_many (data->fileSource, data->decoder, data->videoQueue, data->videoRate,
				data->videoCapsFilter, data->videoEncoder, data->videoRtpPay, data->videoUdpSink, NULL)) {
					g_printerr ("Failed to link passive video streaming pipeline\n");
		}
	}
}

void GstServer::setPipelineToRun(GstData *data) {
	GstStateChangeReturn ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (data->pipeline);
	} 
	else {
		g_print("Waiting for client.\n");	
	}
}

void GstServer::waitForEosOrError(void *voidData) {
	GstData * data = (GstData *)voidData;
	GstBus *bus = gst_element_get_bus (data->pipeline);
	GstMessage *message = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, 
		(GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
	if (message != NULL) {
		GError *error;
		gchar *debug_info;
	
		switch (GST_MESSAGE_TYPE (message)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error (message, &error, &debug_info);
				g_printerr ("Error received from element %s: %s.\n", 
					GST_OBJECT_NAME (message->src), error->message);
				g_printerr ("Debugging information: %s.\n", 
					debug_info ? debug_info : "none");
				g_clear_error (&error);
				g_free (debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print ("End-Of-Stream reached.\n");
      			break;
			default:
				g_printerr ("Unexpected message received.\n");
		        break;
		}
		gst_message_unref (message);
	}
	gst_object_unref (bus);
	_endthread();
}

void GstServer::stopAndFreeResources(GstData *data) {
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	gst_object_unref (data->pipeline);
}

void GstServer::playPipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}

void GstServer::pausePipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
}

void GstServer::stopPipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_READY);
}