#include <gst/gst.h>

int main(int argc, char *argv[]) {
	const gchar* clientIp = "127.0.0.1";
	const gint clientPort = 5000;
	GstElement *pipeline, *videoSource, *videoEncoder, *videoRtpPay, *videoUdpSink;

	gst_init (&argc, &argv);
	
	videoSource = gst_element_factory_make ("videotestsrc", "videoSource");
	videoEncoder = gst_element_factory_make ("ffenc_mjpeg", "videoEncoder");
	videoRtpPay = gst_element_factory_make ("rtpjpegpay", "videoRtpPay");
	videoUdpSink = gst_element_factory_make ("udpsink", "videoUdpSink");
	pipeline = gst_pipeline_new ("streaming_server_pipeline");
	
	g_object_set (videoUdpSink, "host", clientIp, "port", clientPort, NULL);
	
	if (!pipeline || !videoSource || !videoEncoder || !videoRtpPay || !videoUdpSink) {
		g_printerr ("Not all elements could be created.\n");
		return -1;
	}

	gst_bin_add_many (GST_BIN (pipeline), videoSource, videoEncoder, videoRtpPay, videoUdpSink, NULL);
	if (!gst_element_link (videoSource, videoEncoder)) {
		g_printerr("Couldn't link: videoSource - encoder.\n");
	}
	if (!gst_element_link (videoEncoder, videoRtpPay)) {
		g_printerr("Couldn't link: encoder - videoRtpPay.\n");
	}	
	if (!gst_element_link (videoRtpPay, videoUdpSink)) {
		g_printerr("Couldn't link: videoRtpPay - videoUdpSink.\n");
	}
	
	GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (pipeline);
		return -1;
	} 
	else {
		g_print("Streaming.\n");	
	}
	
	GstBus *bus = gst_element_get_bus (pipeline);
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
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (pipeline);
	return 0;
}
