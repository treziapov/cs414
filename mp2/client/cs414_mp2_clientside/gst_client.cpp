#define ACTIVE 1
#define PASSIVE 2

#include <process.h>

#include "gst_client.h"

void GstClient::initPipeline(GstData *data, int videoPort, int audioPort) {
	gst_init (NULL, NULL);
	
	data->videoUdpSource = gst_element_factory_make ("udpsrc", "videoUdpSource");
	data->videoUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->videoRtpDepay = gst_element_factory_make ("rtpjpegdepay", "videoRtpDepay");
	data->videoDecoder = gst_element_factory_make ("ffdec_mjpeg", "videoDecoder");
<<<<<<< HEAD
	data->videoSink = gst_element_factory_make ("autovideosink", "videoSink");
	/*data->videoQueue = gst_element_factory_make ("queue", "videoQueue");
	data->videoTee = gst_element_factory_make("tee", "videoTee");
	data->videoAppSink = gst_element_factory_make("appsink", "videoAppSink");
	data->videoAppQueue = gst_element_factory_make("queue", "videoAppQueue");
	data->videoDecQueue = gst_element_factory_make("queue", "videoDecQueue");
	data->videoDecAppQueue = gst_element_factory_make("queue", "videoDecAppQueue");*/
=======
	data->videoSink = gst_element_factory_make ("d3dvideosink", "videoSink");
	data->videoQueue = gst_element_factory_make ("queue", "videoQueue");
>>>>>>> 4d1aeb1c7faa4500b5c36ccd7e063a3a10c605a1
	
	data->audioUdpSource = gst_element_factory_make("udpsrc", "audioUdpSource");
	data->audioUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->audioRtpDepay = gst_element_factory_make ("rtppcmudepay", "audioRtpDepay");
	data->audioDecoder = gst_element_factory_make ("mulawdec", "audioDecoder");
	data->audioSink = gst_element_factory_make ("autoaudiosink", "audioSink");
	/*data->audioQueue = gst_element_factory_make ("queue", "audioQueue");
	data->audioTee = gst_element_factory_make("tee", "audioTee");
	data->audioAppSink = gst_element_factory_make("appsink", "audioAppSink");
	data->audioAppQueue = gst_element_factory_make("queue", "audioAppQueue");*/
	
	/*data->jitterTee = gst_element_factory_make("tee", "jitterTee");
	data->jitterQueue = gst_element_factory_make("queue", "jitterQueue");
	data->jitterAppSink = gst_element_factory_make("appsink", "jitterAppSink");
	data->jitterBuffer = gst_element_factory_make("gstrtpjitterbuffer", "jitterBuffer");*/
	data->pipeline = gst_pipeline_new ("streaming_client_pipeline");
	
	char videoUri[32], audioUri[32];
	sprintf (videoUri, "udp://localhost:%d", videoPort);
	sprintf (audioUri, "udp://localhost:%d", audioPort);
	g_object_set (data->videoUdpSource, "uri", videoUri, "caps", data->videoUdpCaps, NULL);
	g_object_set (data->audioUdpSource, "uri", audioUri, "caps", data->audioUdpCaps, NULL);
	g_object_set (data->jitterBuffer, "do-lost", true, NULL);

	printf("Streaming video from port %d and audio from port %d\n", videoPort, audioPort);
	
	if (!data->pipeline ||
		!data->videoUdpSource || !data->videoUdpCaps || !data->videoRtpDepay || !data->videoDecoder || !data->videoSink || !data->videoQueue || !data->videoAppSink || !data->videoAppQueue || !data->videoDecQueue || !data->videoDecAppQueue ||
		!data->audioUdpSource || !data->audioUdpCaps || !data->audioRtpDepay || !data->audioDecoder || !data->audioSink || !data->audioAppSink || !data-> audioAppQueue || 
		!data->jitterBuffer || !data->jitterTee || !data->jitterAppSink || !data->jitterQueue) {
			g_printerr ("Not all elements could be created.\n");
	}

	//g_object_set (data->videoAppSink, "emit-signals", true, "caps", data->videoUdpCaps, NULL);	
	//g_signal_connect (data->videoAppSink, "new-buffer", G_CALLBACK (newVideoBuffer), data);

	//g_object_set (data->jitterAppSink, "emit-signals", true, "caps", data->videoUdpCaps, NULL);	
	//g_signal_connect (data->jitterAppSink, "new-buffer", G_CALLBACK (jitterBuffer), data);

	//g_object_set (data->audioAppSink, "emit-signals", true, "caps", data->audioUdpCaps, NULL);	
	//g_signal_connect (data->audioAppSink, "new-buffer", G_CALLBACK (newAudioBuffer), data);
}

void GstClient::buildPipeline(GstData *data) {
	if (data->mode == PASSIVE) {
		printf("passive\n");
		gst_bin_add_many (GST_BIN (data->pipeline), 
			data->videoUdpSource, data->videoRtpDepay, data->videoDecoder, data->videoSink, data->videoQueue, data->videoTee, data->videoDecQueue, data->videoDecAppQueue, data->videoAppSink,
			data->jitterBuffer, data->jitterTee, data->jitterQueue, data->jitterAppSink,
			NULL);
		//This is the simple pipeline
		if (!gst_element_link (data->videoUdpSource, data->jitterBuffer)) {
			g_printerr("Couldn't link: videoUdpSource - jitterBuffer.\n");
		}	
		if(!gst_element_link (data->jitterBuffer, data->videoRtpDepay)) {
			g_printerr("Couldn't link: jitterBuffer - videoRtpDepay.\n");
		}
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (data->videoDecoder, data->videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
		/* This is the VIDEO pipeline with the tees and appsinks
		//Linking pipeline
		//Source to buffer
		if (!gst_element_link (data->videoUdpSource, data->jitterBuffer)) {
			g_printerr("Couldn't link: videoUdpSource - jitterBuffer.\n");
		}	
		//Split pipeline with tee so we can use jitter
		if(!gst_element_link (data->jitterBuffer, data->jitterTee)) {
			g_printerr("Couldn't link: jitterBuffer - jitterTee.\n");
		}
		if(!gst_element_link (data->jitterTee, data->videoQueue)) {
			g_printerr("Couldn't link: jitterTee - videoQueue.\n");
		}
		if(!gst_element_link (data->jitterTee, data->jitterQueue)) {
			g_printerr("Couldn't link: jitterTee - jitterQueue.\n");
		}
		if(!gst_element_link (data->jitterQueue, data->jitterAppSink)) {
			g_printerr("Couldn't link: jitterQueue - jitterAppSink.\n");
		}
		if(!gst_element_link (data->videoQueue, data->videoRtpDepay)) {
			g_printerr("Couldn't link: videoQueue - videoRtpDepay.\n");
		}
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		//Split pipeline with tee so we can have an appsink
		if(!gst_element_link (data->videoDecoder, data->videoTee)) {
			g_printerr("Couldn't link: videoDecoder - videoTee.\n");
		}
		if(!gst_element_link (data->videoTee, data->videoDecQueue)) {
			g_printerr("Couldn't link: videoTee - videoDecQueue.\n");
		}
		if(!gst_element_link (data->videoTee, data->videoDecAppQueue)) {
			g_printerr("Couldn't link: videoTee - videoDecAppQueue.\n");
		}
		if(!gst_element_link (data->videoDecAppQueue, data->videoAppSink)) {
			g_printerr("Couldn't link: videoDecAppQueue - videoAppSink.\n");
		}
		if(!gst_element_link (data->videoDecQueue, data->videoSink)) {
			g_printerr("Couldn't link: videoDecQueue - videoDecoder.\n");
		}*/
	}
	else if (data->mode == ACTIVE) {
		gst_bin_add_many (GST_BIN (data->pipeline),
			data->videoUdpSource, data->videoRtpDepay, data->videoDecoder, data->videoSink,
			data->videoQueue, data->videoTee, data->videoDecQueue, data->videoDecAppQueue, data->videoAppSink,
			data->audioUdpSource, data->audioRtpDepay, data->audioDecoder, data->audioSink,
			data->audioQueue, data->audioAppSink, data->audioTee, data->audioAppQueue,
			data->jitterBuffer, data->jitterTee, data->jitterQueue, data->jitterAppSink,
			 NULL);
		if (!gst_element_link (data->videoUdpSource, data->jitterBuffer)) {
			g_printerr("Couldn't link: videoUdpSource - jitterBuffer.\n");
		}
		if (!gst_element_link (data->jitterBuffer, data->videoRtpDepay)) {
			g_printerr("Couldn't link: jitterBuffer - videoRtpDepay.\n");
		}
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		if (!gst_element_link (data->videoDecoder, data->videoSink)) {
			g_printerr("Couldn't link: videoDecoder - videoSink.\n");
		}
		/* This is the VIDEO pipeline with the tees and appsinks
		//Linking pipeline
		//Source to buffer
		if (!gst_element_link (data->videoUdpSource, data->jitterBuffer)) {
			g_printerr("Couldn't link: videoUdpSource - jitterBuffer.\n");
		}	
		//Split pipeline with tee so we can use jitter
		if(!gst_element_link (data->jitterBuffer, data->jitterTee)) {
			g_printerr("Couldn't link: jitterBuffer - jitterTee.\n");
		}
		if(!gst_element_link (data->jitterTee, data->videoQueue)) {
			g_printerr("Couldn't link: jitterTee - videoQueue.\n");
		}
		if(!gst_element_link (data->jitterTee, data->jitterQueue)) {
			g_printerr("Couldn't link: jitterTee - jitterQueue.\n");
		}
		if(!gst_element_link (data->jitterQueue, data->jitterAppSink)) {
			g_printerr("Couldn't link: jitterQueue - jitterAppSink.\n");
		}
		if(!gst_element_link (data->videoQueue, data->videoRtpDepay)) {
			g_printerr("Couldn't link: videoQueue - videoRtpDepay.\n");
		}
		if (!gst_element_link (data->videoRtpDepay, data->videoDecoder)) {
			g_printerr("Couldn't link: videoRtpDepay - videoDecoder.\n");
		}
		//Split pipeline with tee so we can have an appsink
		if(!gst_element_link (data->videoDecoder, data->videoTee)) {
			g_printerr("Couldn't link: videoDecoder - videoTee.\n");
		}
		if(!gst_element_link (data->videoTee, data->videoDecQueue)) {
			g_printerr("Couldn't link: videoTee - videoDecQueue.\n");
		}
		if(!gst_element_link (data->videoTee, data->videoDecAppQueue)) {
			g_printerr("Couldn't link: videoTee - videoDecAppQueue.\n");
		}
		if(!gst_element_link (data->videoDecAppQueue, data->videoAppSink)) {
			g_printerr("Couldn't link: videoDecAppQueue - videoAppSink.\n");
		}
		if(!gst_element_link (data->videoDecQueue, data->videoSink)) {
			g_printerr("Couldn't link: videoDecQueue - videoDecoder.\n");
		}*/
		if (!gst_element_link (data->audioUdpSource, data->audioRtpDepay)) {
			g_printerr("Couldn't link: audioUdpSource - audioRtpDepay.\n");
		}	
		if (!gst_element_link (data->audioRtpDepay, data->audioDecoder)) {
			g_printerr("Couldn't link: audioRtpDepay - audioDecoder.\n");
		}
		if (!gst_element_link (data->audioDecoder, data->audioSink)) {
			g_printerr("Couldn't link: audioDecoder - audioSink.\n");
		}
		/* This is the AUDIO pipeline with tees and sinks		
		if (!gst_element_link (data->audioUdpSource, data->audioRtpDepay)) {
			g_printerr("Couldn't link: audioUdpSource - audioRtpDepay.\n");
		}	
		if (!gst_element_link (data->audioRtpDepay, data->audioDecoder)) {
			g_printerr("Couldn't link: audioRtpDepay - audioDecoder.\n");
		}
		if (!gst_element_link (data->audioDecoder, data->audioTee)) {
			g_printerr("Couldn't link: audioDecoder - audioTee.\n");
		}
		if (!gst_element_link (data->audioTee, data->audioAppQueue)) {
			g_printerr("Couldn't link: audioTee - audioAppQueue.\n");
		}
		if (!gst_element_link (data->audioAppQueue, data->audioAppSink)) {
			g_printerr("Couldn't link: audioAppQueue - audioAppSink.\n");
		}
		if (!gst_element_link (data->audioTee, data->audioQueue)) {
			g_printerr("Couldn't link: audioTee - audioQueue.\n");
		}
		if (!gst_element_link (data->audioQueue, data->audioSink)) {
			g_printerr("Couldn't link: audioQueue - audioSink.\n");
		}*/
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

void GstClient::waitForEosOrError(void *voidData) {
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

void GstClient::stopAndFreeResources(GstData *data) {
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	gst_object_unref (data->pipeline);
}

/*
	Send a seek event for navigating the video content
	i.e. fast-forwarding, rewinding
*/
void sendSeekEvent(GstData *data) {
	gint64 position;
	GstFormat format = GST_FORMAT_TIME;
	GstEvent *seek_event;
   
	// Obtain the current position, needed for the seek event
	if (!gst_element_query_position (data->pipeline, &format, &position)) {
		g_printerr ("Unable to retrieve current position.\n");
		return;
	}
  
	// Create the seek event
	if (data->playbackRate > 0) {
		seek_event = gst_event_new_seek (data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0);
	} 
	else {
		seek_event = gst_event_new_seek (data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
	}

	// Send the event
	gst_element_send_event (data->videoSink, seek_event);
	g_print ("Current playbackRate: %g\n", data->playbackRate);
}

void GstClient::playPipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}

void GstClient::pausePipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
}

void GstClient::stopPipeline(GstData *data) {
	gst_element_set_state(data->pipeline, GST_STATE_READY);
}

void GstClient::rewindVideo(GstData *data) {
	if (data->playbackRate > 0) {
		data->playbackRate = -1.0;
	}
	else {
		data->playbackRate *= 2.0;
	}
	sendSeekEvent(data);
	setPipelineToRun(data);
}

void GstClient::fastForwardVideo(GstData *data) {
	if (data->playbackRate < 0) {
		data->playbackRate = 1.0;
	}
	else {
		data->playbackRate *= 2.0;
	}
	sendSeekEvent(data);
	setPipelineToRun(data);
}