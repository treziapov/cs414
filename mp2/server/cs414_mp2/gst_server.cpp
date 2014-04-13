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
		curr++;
	}
	
	char path[124];
	strcpy (path, home);
	strcat (path, directory);
	strcat (path, filename);
	g_print ("getFilePathInHomeDirectory: generated path - %s\n", path);
	return strdup(path);
}

static void decoderPadAdded_handler (GstElement *source, GstPad *newPad, GstData *data) {
	g_print ("decoderPadAdded_handler: Received new pad '%s' from '%s'.\n", 
		GST_PAD_NAME(newPad), GST_ELEMENT_NAME(source));

	// Check new pad's type
	GstCaps *newPadCaps = gst_pad_get_caps (newPad);
	GstStructure *newPadStruct = gst_caps_get_structure (newPadCaps, 0);
	const gchar *newPadType = gst_structure_get_name (newPadStruct);
	g_print ("\t New pad type - %s.\n", newPadType);

	if (g_str_has_prefix (newPadType, "video/x-raw")) {
		GstPad *videoQueueSinkPad = gst_element_get_static_pad (data->videoQueue, "sink");
		if (gst_pad_is_linked (videoQueueSinkPad)) {
			g_print ("\t Video already linked.\n");
			return;
		}
		GstPadLinkReturn ret = gst_pad_link (newPad, videoQueueSinkPad);
		if (GST_PAD_LINK_FAILED (ret)) {
			g_print ("\t Type is '%s' but link failed.\n", newPadType);
		}
		else {
			g_print ("\t Link succeeded, type - '%s'.\n", newPadType);
		}
		gst_object_unref (videoQueueSinkPad);
	}
	else if (g_str_has_prefix (newPadType, "audio/x-raw")) {
		GstPad *audioQueueSinkPad = gst_element_get_static_pad (data->audioQueue, "sink");
		if (gst_pad_is_linked (audioQueueSinkPad)) {
			g_print ("\t Audio already linked.\n");
			return;
		}
		GstPadLinkReturn ret = gst_pad_link (newPad, audioQueueSinkPad);
		if (GST_PAD_LINK_FAILED (ret)) {
			g_print ("\t Type is '%s' but link failed.\n", newPadType);
		}
		else {
			g_print ("\t Link succeeded, type - '%s'.\n", newPadType);
		}
		gst_object_unref (audioQueueSinkPad);
	}
	if (newPadCaps != NULL) {
		gst_caps_unref (newPadCaps);
	}
}

void GstServer::initPipeline(GstData *data) {
	gst_init (NULL, NULL);

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

	data->pipeline = gst_pipeline_new ("streaming_server_pipeline");

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

	g_print ("configurePipeline: streaming file '%s' to %s, on ports %d and %d.\n", 
		videoFilePath, data->clientIp, data->videoPort, data->audioPort);

	delete videoFilePath;
	g_object_unref (videoRateCaps);
}

void GstServer::buildPipeline(GstData *data) {
	if(data->mode == Active) {
		gst_bin_add_many (
			GST_BIN (data->pipeline), 
			data->fileSource, data->decoder,
			data->videoQueue, data->videoRate, data->videoCapsFilter, 
			data->videoEncoder, data->videoRtpPay, data->videoUdpSink,
			data->audioQueue, data->audioEncoder, data->audioRtpPay, data->audioUdpSink, NULL);
		if (!gst_element_link_many(data->audioQueue, data->audioEncoder, 
				data->audioRtpPay, data->audioUdpSink, NULL)) {
					g_printerr ("Failed to link audio streaming pipeline.\n");
		}
	}
	else {
		gst_bin_add_many (
			GST_BIN (data->pipeline), 
			data->fileSource, data->decoder,
			data->videoQueue, data->videoRate, data->videoCapsFilter, 
			data->videoEncoder, data->videoRtpPay, data->videoUdpSink, NULL);
	}

	g_signal_connect (data->decoder, "pad-added", G_CALLBACK (decoderPadAdded_handler), data);

	if (!gst_element_link_many(data->videoQueue, data->videoRate, 
			data->videoCapsFilter, data->videoEncoder, data->videoRtpPay, data->videoUdpSink, NULL)) {
				g_printerr ("Failed to link video streaming pipeline.\n");
	}

	if (!gst_element_link (data->fileSource, data->decoder)) {
		g_printerr ("Failed to link fileSource with decoder.\n");
	}
}

void GstServer::setPipelineToRun(GstData *data) {
	GstStateChangeReturn ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (data->pipeline);
	} 
	else {
		g_print("Pipeline is PLAYING.\n");	
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
	int ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to PLAY.\n");
	} 
	else {
		g_print("Pipeline is PLAYING.\n");	
	}
}

void GstServer::pausePipeline(GstData *data) {
	int ret = gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to PAUSE.\n");
	} 
	else {
		g_print ("Pipeline is PAUSED\n");
	}
}

void GstServer::stopPipeline(GstData *data) {
	int ret = gst_element_set_state (data->pipeline, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to STOP.\n");
	} 
	else {
		g_print ("Pipeline is STOPPED\n");
	}
	gst_object_unref (data->pipeline);
}