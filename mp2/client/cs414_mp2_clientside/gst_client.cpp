#include "gst_client.h"

void GstClient::initPipeline(GstData *data) {
	gst_init (NULL, NULL);
	
	data->videoUdpSource = gst_element_factory_make ("udpsrc", "videoUdpSource");
	data->videoUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->videoRtpDepay = gst_element_factory_make ("rtpjpegdepay", "videoRtpDepay");
	data->videoDecoder = gst_element_factory_make ("ffdec_mjpeg", "videoDecoder");
	data->videoSink = gst_element_factory_make ("autovideosink", "videoSink");
	
	data->audioUdpSource = gst_element_factory_make("udpsrc", "audioUdpSource");
	data->audioUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->audioRtpDepay = gst_element_factory_make ("rtppcmudepay", "audioRtpDepay");
	data->audioDecoder = gst_element_factory_make ("mulawdec", "audioDecoder");
	data->audioSink = gst_element_factory_make ("autoaudiosink", "audioSink");
	
	data->pipeline = gst_pipeline_new ("streaming_client_pipeline");
	
	char videoUri[32], audioUri[32];
	sprintf (videoUri, "udp://localhost:%d", VIDEO_PORT);
	sprintf (audioUri, "udp://localhost:%d", AUDIO_PORT);
	g_object_set (data->videoUdpSource, "uri", videoUri, "caps", data->videoUdpCaps, NULL);
	g_object_set (data->audioUdpSource, "uri", audioUri, "caps", data->audioUdpCaps, NULL);
	
	if (!data->pipeline ||
		!data->videoUdpSource || !data->videoUdpCaps || !data->videoRtpDepay || 
		!data->videoDecoder || !data->videoSink ||
		!data->audioUdpSource || !data->audioUdpCaps || !data->audioRtpDepay || 
		!data->audioDecoder || !data->audioSink) {
			g_printerr ("Not all elements could be created.\n");
	}
}

void GstClient::buildPipeline(GstData *data) {
	if (data->mode == Passive) {
		gst_bin_add_many (GST_BIN (data->pipeline), 
			data->videoUdpSource, data->videoRtpDepay, data->videoDecoder,data-> videoSink, NULL);
		if (!gst_element_link (data->videoUdpSource, data->videoRtpDepay)) {
			g_printerr("Couldn't link: videoUdpSource - videoRtpDepay.\n");
		}	
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (data->videoDecoder, data->videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
	}
	else if (data->mode == Active) {
		gst_bin_add_many (GST_BIN (data->pipeline),
			data->videoUdpSource, data->videoRtpDepay, data->videoDecoder, data->videoSink, 
			data->audioUdpSource, data->audioRtpDepay, data->audioDecoder, data->audioSink, NULL);
		if (!gst_element_link (data->videoUdpSource, data->videoRtpDepay)) {
			g_printerr("Couldn't link: videoUdpSource - videoRtpDepay.\n");
		}	
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (data->videoDecoder, data->videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
		if (!gst_element_link (data->audioUdpSource, data->audioRtpDepay)) {
			g_printerr("Couldn't link: audioUdpSource - audioRtpDepay.\n");
		}	
		if (!gst_element_link (data->audioRtpDepay, data->audioDecoder)) {
			g_printerr("Couldn't link: audioRtpDepay - audioDecoder.\n");
		}
		if (!gst_element_link (data->audioDecoder, data->audioSink)) {
			g_printerr("Couldn't link: audioDecoder - audioSink.\n");
		}
	}
}

void GstClient::setPipelineToRun(GstData *data) {
	GstStateChangeReturn ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (data->pipeline);
	} 
	else {
		g_print("Waiting for stream.\n");	
	}
}

void GstClient::waitForEosOrError(GstData *data) {
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

void GstClient::stopAndFreeResources(GstData *data) {
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	gst_object_unref (data->pipeline);
}