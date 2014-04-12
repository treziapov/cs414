#include <gst/gst.h>

enum Mode {
	Active, 
	Passive
};

Mode mode;
const gint videoPort = 5000;
const gint audioPort = 5001;
GstElement *pipeline;
GstElement *videoUdpSource, *videoRtpDepay, *videoDecoder, *videoSink;
GstElement *audioUdpSource, *audioRtpDepay, *audioDecoder, *audioSink;
GstCaps *videoUdpCaps, *audioUdpCaps;

void buildPipelineForMode(Mode mode, GstElement *pipeline) {
	if (mode == Passive) {
		gst_bin_add_many (GST_BIN (pipeline), 
			videoUdpSource, videoRtpDepay, videoDecoder, videoSink, NULL);
		if (!gst_element_link (videoUdpSource, videoRtpDepay)) {
			g_printerr("Couldn't link: videoUdpSource - videoRtpDepay.\n");
		}	
		if (!gst_element_link (videoRtpDepay, videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (videoDecoder, videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
	}
	else if (mode == Active) {
		gst_bin_add_many (GST_BIN (pipeline),
			videoUdpSource, videoRtpDepay, videoDecoder, videoSink, 
			audioUdpSource, audioRtpDepay, audioDecoder, audioSink, NULL);
		if (!gst_element_link (videoUdpSource, videoRtpDepay)) {
			g_printerr("Couldn't link: videoUdpSource - videoRtpDepay.\n");
		}	
		if (!gst_element_link (videoRtpDepay, videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (videoDecoder, videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
		if (!gst_element_link (audioUdpSource, audioRtpDepay)) {
			g_printerr("Couldn't link: audioUdpSource - audioRtpDepay");
		}	
		if (!gst_element_link (audioRtpDepay, audioDecoder)) {
			g_printerr("Couldn't link: audioRtpDepay - audioDecoder");
		}
		if (!gst_element_link (audioDecoder, audioSink)) {
			g_printerr("Couldn't link: audioDecoder - audioSink.\n");
		}
	}
}

int main(int argc, char *argv[]) {
	mode = Active;
	gst_init (&argc, &argv);
	
	videoUdpSource = gst_element_factory_make ("udpsrc", "videoUdpSource");
	videoUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	videoRtpDepay = gst_element_factory_make ("rtpjpegdepay", "videoRtpDepay");
	videoDecoder = gst_element_factory_make ("ffdec_mjpeg", "videoDecoder");
	videoSink = gst_element_factory_make ("autovideosink", "videoSink");
	
	audioUdpSource = gst_element_factory_make("udpsrc", "audioUdpSource");
	audioUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	audioRtpDepay = gst_element_factory_make ("rtppcmudepay", "audioRtpDepay");
	audioDecoder = gst_element_factory_make ("mulawdec", "audioDecoder");
	audioSink = gst_element_factory_make ("autoaudiosink", "audioSink");
	
	pipeline = gst_pipeline_new ("streaming_client_pipeline");
	
	g_object_set (videoUdpSource, "port", videoPort, "caps", videoUdpCaps, NULL);
	g_object_set (audioUdpSource, "port", audioPort, "caps", audioUdpCaps, NULL);
	
	if (!pipeline ||
		!videoUdpSource || !videoUdpCaps || !videoRtpDepay || !videoDecoder || !videoSink ||
		!audioUdpSource || !audioUdpCaps || !audioRtpDepay || !audioDecoder || !audioSink) {
			g_printerr ("Not all elements could be created.\n");
			return -1;
	}

	buildPipelineForMode(mode, pipeline);
	
	GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (pipeline);
		return -1;
	} 
	else {
		g_print("Waiting for stream.\n");	
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
