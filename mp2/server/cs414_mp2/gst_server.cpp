#include "gst_server.h"

void GstServer::initPipeline(GstData *data, int videoPort, int audioPort) {
	gst_init (NULL, NULL);
	
	data->videoSource = gst_element_factory_make ("videotestsrc", "videoSource");
	data->videoEncoder = gst_element_factory_make ("ffenc_mjpeg", "videoEncoder");
	data->videoRtpPay = gst_element_factory_make ("rtpjpegpay", "videoRtpPay");
	data->videoUdpSink = gst_element_factory_make ("udpsink", "videoUdpSink");
	
	data->audioSource = gst_element_factory_make ("audiotestsrc", "audioSource");
	data->audioEncoder = gst_element_factory_make ("mulawenc", "audioEncoder");
	data->audioRtpPay = gst_element_factory_make ("rtppcmupay", "audioRtpPay");
	data->audioUdpSink = gst_element_factory_make ("udpsink", "audioUdpSink");
	
	data->pipeline = gst_pipeline_new ("streaming_server_pipeline");
	
	g_object_set (data->videoUdpSink, "host", data->clientIp, "port", videoPort, NULL);
	g_object_set (data->audioUdpSink, "host", data->clientIp, "port", audioPort, NULL);

	printf("Streaming video to port %d, and audio to port %d\n", videoPort, audioPort);
	
	if (!data->pipeline || !data->videoSource || !data->videoEncoder || 
		!data->videoRtpPay || !data->videoUdpSink) {
			g_printerr ("Not all video elements could be created.\n");
	}
	if (!data->pipeline || !data->audioSource || !data->audioEncoder || 
		!data->audioRtpPay || !data->audioUdpSink) {
			g_printerr ("Not all audio elements could be created.\n");
	}
}

void GstServer::buildPipeline(GstData *data) {
	gst_bin_add_many (GST_BIN (data->pipeline), 
		data->videoSource, data->videoEncoder, data->videoRtpPay, data->videoUdpSink,
		data->audioSource, data->audioEncoder, data->audioRtpPay, data->audioUdpSink, NULL);
	if (!gst_element_link (data->videoSource, data->videoEncoder)) {
		g_printerr("Couldn't link: videoSource - encoder.\n");
	}
	if (!gst_element_link (data->videoEncoder, data->videoRtpPay)) {
		g_printerr("Couldn't link: encoder - videoRtpPay.\n");
	}	
	if (!gst_element_link (data->videoRtpPay, data->videoUdpSink)) {
		g_printerr("Couldn't link: videoRtpPay - videoUdpSink.\n");
	}
	if (!gst_element_link (data->audioSource, data->audioEncoder)) {
		g_printerr("Couldn't link: audioSource - audioEncoder.\n");
	}
	if (!gst_element_link (data->audioEncoder, data->audioRtpPay)) {
		g_printerr("Couldn't link: audioEncoder - audioRtpPay.\n");
	}	
	if (!gst_element_link (data->audioRtpPay, data->audioUdpSink)) {
		g_printerr("Couldn't link: audioRtpPay - audioUdpSink.\n");
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

void GstServer::waitForEosOrError(GstData *data) {
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
}

void GstServer::stopAndFreeResources(GstData *data) {
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	gst_object_unref (data->pipeline);
}