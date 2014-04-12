#include <gst/gst.h>

int main(int argc, char *argv[]) {
	const gchar* clientIp = "127.0.0.1";
	const gint clientVideoPort = 5000;
	const gint clientAudioPort = 5001;
	
	GstElement *videoPipeline, *videoSource, *videoEncoder, *videoRtpPay, *videoUdpSink;
	GstElement *audioPipeline, *audioSource, *audioEncoder, *audioRtpPay, *audioUdpSink;

	gst_init (&argc, &argv);
	
	videoSource = gst_element_factory_make ("videotestsrc", "videoSource");
	videoEncoder = gst_element_factory_make ("ffenc_mjpeg", "videoEncoder");
	videoRtpPay = gst_element_factory_make ("rtpjpegpay", "videoRtpPay");
	videoUdpSink = gst_element_factory_make ("udpsink", "videoUdpSink");
	videoPipeline = gst_pipeline_new ("streaming_server_video_pipeline");
	
	audioSource = gst_element_factory_make ("audiotestsrc", "audioSource");
	audioEncoder = gst_element_factory_make ("mulawenc", "audioEncoder");
	audioRtpPay = gst_element_factory_make ("rtppcmupay", "audioRtpPay");
	audioUdpSink = gst_element_factory_make ("udpsink", "audioUdpSink");
	audioPipeline = gst_pipeline_new ("streamer_server_audio_pipeline");
	
	g_object_set (videoUdpSink, "host", clientIp, "port", clientVideoPort, NULL);
	g_object_set (audioUdpSink, "host", clientIp, "port", clientAudioPort, NULL);
	
	if (!videoPipeline || !videoSource || !videoEncoder || !videoRtpPay || !videoUdpSink) {
		g_printerr ("Not all video elements could be created.\n");
	}
	if (!audioPipeline || !audioSource || !audioEncoder || !audioRtpPay || !audioUdpSink) {
		g_printerr ("Not all audio elements could be created.\n");
	}

	gst_bin_add_many (GST_BIN (videoPipeline), 
		videoSource, videoEncoder, videoRtpPay, videoUdpSink, NULL);
	if (!gst_element_link (videoSource, videoEncoder)) {
		g_printerr("Couldn't link: videoSource - encoder.\n");
	}
	if (!gst_element_link (videoEncoder, videoRtpPay)) {
		g_printerr("Couldn't link: encoder - videoRtpPay.\n");
	}	
	if (!gst_element_link (videoRtpPay, videoUdpSink)) {
		g_printerr("Couldn't link: videoRtpPay - videoUdpSink.\n");
	}
	
	gst_bin_add_many (GST_BIN (audioPipeline), 
		audioSource, audioEncoder, audioRtpPay, audioUdpSink, NULL);
	if (!gst_element_link (audioSource, audioEncoder)) {
		g_printerr("Couldn't link: audioSource - audioEncoder.\n");
	}
	if (!gst_element_link (audioEncoder, audioRtpPay)) {
		g_printerr("Couldn't link: audioEncoder - audioRtpPay.\n");
	}	
	if (!gst_element_link (audioRtpPay, audioUdpSink)) {
		g_printerr("Couldn't link: audioRtpPay - audioUdpSink.\n");
	}
	
	GstStateChangeReturn ret = gst_element_set_state (videoPipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the videoPipeline to the playing state.\n");
		g_object_unref (videoPipeline);
		return -1;
	} 
	else {
		g_print("Streaming video.\n");	
	}
	
	ret = gst_element_set_state (audioPipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the audioPipeline to the playing state.\n");
		g_object_unref (audioPipeline);
		return -1;
	} 
	else {
		g_print("Streaming audio.\n");	
	}
	
	GstBus *bus = gst_element_get_bus (videoPipeline);
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
	gst_element_set_state (videoPipeline, GST_STATE_NULL);
	gst_object_unref (videoPipeline);
	return 0;
}
