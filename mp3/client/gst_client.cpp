#define ACTIVE 1
#define PASSIVE 2

#include <pthread.h>
#include <string.h>

#include "gst_client.h"

const gchar* GstClient::projectDirectory = "/Documents/GitHub/cs414/mp2/client/cs414_mp2_clientside/";

SinkData * globalDataServer1;
SinkData * globalDataServer2;
int flag_first_or_second_server = 0;

char* GstClient::getFilePathInHomeDirectory(const char* directory, const char* filename) {
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

void newVideoBuffer(GstElement * sink, GstData * data){
	SinkData * sinkData = (SinkData *)data;
	GstBuffer * buffer;
	g_signal_emit_by_name(sink, "pull-buffer", &buffer);
	if(buffer){
		GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
		GstClockTime playTimestamp = gst_clock_get_time(gst_element_get_clock(sink));
		sinkData->videoTSD = (playTimestamp - timestamp) / GST_MSECOND;
	}
}

void newAudioBuffer(GstElement * sink, GstData * data){
	SinkData * sinkData = (SinkData *)data;
	GstBuffer * buffer;
	g_signal_emit_by_name(sink, "pull-buffer", &buffer);
	if(buffer){
		GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
		GstClockTime playTimestamp = gst_clock_get_time(gst_element_get_clock(sink));
		sinkData->audioTSD = (playTimestamp - timestamp) / GST_MSECOND;
	}
}

void jitterBuffer(GstElement * sink, GstData * data){
	SinkData * sinkData = (SinkData *)data;
	GstBuffer * buffer;
	g_signal_emit_by_name(sink, "pull-buffer", &buffer);
	if(buffer){
		GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
		GstClockTime decodeTimestamp = gst_clock_get_time(gst_element_get_clock(sink));
		sinkData->ping = (GstClockTime)(((unsigned int)timestamp - (unsigned int)decodeTimestamp) / (unsigned int)GST_MSECOND);
        printf("decodeTimestamp: %u timestamp: %u\n", (unsigned int)decodeTimestamp, (unsigned int)timestamp);
	}

	sinkData->successes++;
}

gboolean jitterEventHandlerServer1(GstPad * pad, GstObject * parent, GstEvent * event){
	
	globalDataServer1->failures++;
	
	return (gboolean)true;
}

gboolean jitterEventHandlerServer2(GstPad * pad, GstObject * parent, GstEvent * event){
	
	globalDataServer2->failures++;
	
	
	return (gboolean)true;
}

void GstClient::initPipeline(GstData *data, int videoPort, int audioPort, SinkData * sinkData) {
	gst_init (NULL, NULL);

	if(flag_first_or_second_server == 0){
		globalDataServer1 = sinkData;
	}
	else{
		globalDataServer2 = sinkData;
	}

	data->videoUdpSource = gst_element_factory_make ("udpsrc", "videoUdpSource");
	data->videoUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->videoRtpDepay = gst_element_factory_make ("rtpjpegdepay", "videoRtpDepay");
	data->videoDecoder = gst_element_factory_make ("ffdec_mjpeg", "videoDecoder");
	data->videoDecCaps = gst_caps_new_simple("video/x-raw-yuv", NULL);
	data->videoTee = gst_element_factory_make("tee", "videoTee");
	data->videoAppSink = gst_element_factory_make("appsink", "videoAppSink");
	data->videoAppQueue = gst_element_factory_make("queue", "videoAppQueue");
	data->videoDecQueue = gst_element_factory_make("queue", "videoDecQueue");
	data->videoDecAppQueue = gst_element_factory_make("queue", "videoDecAppQueue");
	data->videoColorspace = gst_element_factory_make("ffmpegcolorspace", "videoColorspace");
	data->videoSink = gst_element_factory_make ("xvimagesink", "videoSink");
	//data->videoSink = gst_element_factory_make ("autovideosink", "videoSink");
	
	data->videoQueue = gst_element_factory_make ("queue", "videoQueue");

	data->audioUdpSource = gst_element_factory_make("udpsrc", "audioUdpSource");
	data->audioUdpCaps = gst_caps_new_simple ("application/x-rtp", NULL);
	data->audioRtpDepay = gst_element_factory_make ("rtppcmudepay", "audioRtpDepay");
	data->audioDecoder = gst_element_factory_make ("mulawdec", "audioDecoder");
	data->audioDecCaps = gst_caps_new_simple("audio/x-raw-int", NULL);
	data->audioSink = gst_element_factory_make ("autoaudiosink", "audioSink");
	data->audioQueue = gst_element_factory_make ("queue", "audioQueue");
	data->audioTee = gst_element_factory_make("tee", "audioTee");
	data->audioAppSink = gst_element_factory_make("appsink", "audioAppSink");
	data->audioAppQueue = gst_element_factory_make("queue", "audioAppQueue");
    data->audioVolume = gst_element_factory_make("volume", "volume");

	data->jitterTee = gst_element_factory_make("tee", "jitterTee");
	data->jitterQueue = gst_element_factory_make("queue", "jitterQueue");
	data->jitterAppSink = gst_element_factory_make("appsink", "jitterAppSink");
	data->jitterBuffer = gst_element_factory_make("gstrtpjitterbuffer", "jitterBuffer");
	data->pipeline = gst_pipeline_new ("streaming_client_pipeline");

	char videoUri[32], audioUri[32];
	sprintf (videoUri, "udp://%s:%d", data->clientIp, videoPort);
	sprintf (audioUri, "udp://%s:%d", data->clientIp, audioPort);
	g_object_set (data->videoUdpSource, "uri", videoUri, "caps", data->videoUdpCaps, NULL);
	g_object_set (data->audioUdpSource, "uri", audioUri, "caps", data->audioUdpCaps, NULL);
	g_object_set (data->jitterBuffer, "do-lost", true, NULL);

	GstPad * appsrcPad = gst_element_get_static_pad(GST_ELEMENT(data->jitterBuffer), "src");
	if(flag_first_or_second_server == 0){
		gst_pad_set_event_function(appsrcPad, (GstPadEventFunction)jitterEventHandlerServer1);
	}
	else{
		gst_pad_set_event_function(appsrcPad, (GstPadEventFunction)jitterEventHandlerServer2);
	}
	printf("Streaming from server '%s', video from port %d and audio from port %d\n", 
		data->clientIp, videoPort, audioPort);

	if (!data->pipeline)
        g_printerr("Could not create pipeline");
    if(!data->videoUdpSource)
        g_printerr("Could not create videoUdpSource");
    if(!data->videoUdpCaps)
        g_printerr("Could not create videoUdpCaps");
    if(!data->videoRtpDepay)
        g_printerr("Could not create videoRtpDepay");
    if(!data->videoDecoder)
        g_printerr("Could not create videoDecoder");
    if(!data->videoSink)
        g_printerr("Could not create videoSink");
    if(!data->videoQueue)
        g_printerr("Could not create videoQueue");
    if(!data->videoAppSink)
        g_printerr("Could not create videoAppSink");
    if(!data->videoAppQueue)
        g_printerr("Could not create videoAppQueue");
    if(!data->videoDecQueue)
        g_printerr("Could not create videoDecQueue");
    if(!data->videoDecAppQueue)
        g_printerr("Could not create videoDecAppQueue");
    if(!data->audioUdpSource)
        g_printerr("Could not create audioUdpSource");
    if(!data->audioUdpCaps)
        g_printerr("Could not create audioUdpCaps");
    if(!data->audioRtpDepay)
        g_printerr("Could not create audioRtpDepay");
    if(!data->audioDecoder)
        g_printerr("Could not create audioDecoder");
    if(!data->audioSink)
        g_printerr("Could not create audioSink");
    if(!data->audioAppSink)
        g_printerr("Could not create audioAppSink");
    if(!data-> audioAppQueue)
        g_printerr("Could not create audioAppQueue");
    if(!data-> audioVolume)
        g_printerr("Could not create audioVolume");
	if(!data->jitterBuffer)
        g_printerr("Could not create jitterBuffer");
    if(!data->jitterTee)
        g_printerr("Could not create jitterTee");
    if(!data->jitterAppSink)
        g_printerr("Could not create jitterAppSink");
    if(!data->jitterQueue)
        g_printerr("Could not create jitterQueue");

	if(flag_first_or_second_server == 0){
		g_object_set (data->videoAppSink, "emit-signals", TRUE, "caps", data->videoDecCaps, NULL);	
		g_signal_connect (data->videoAppSink, "new-buffer", G_CALLBACK (newVideoBuffer), globalDataServer1);

		g_object_set (data->jitterAppSink, "emit-signals", TRUE, "caps", data->videoUdpCaps, NULL);	
		g_signal_connect (data->jitterAppSink, "new-buffer", G_CALLBACK (jitterBuffer), globalDataServer1);

		if (data->mode == Active) {
			g_object_set (data->audioAppSink, "emit-signals", true, "caps", data->audioDecCaps, NULL);	
			g_signal_connect (data->audioAppSink, "new-buffer", G_CALLBACK (newAudioBuffer), globalDataServer1);
		}
	}

	else{
		g_object_set (data->videoAppSink, "emit-signals", TRUE, "caps", data->videoDecCaps, NULL);	
		g_signal_connect (data->videoAppSink, "new-buffer", G_CALLBACK (newVideoBuffer), globalDataServer2);

		g_object_set (data->jitterAppSink, "emit-signals", TRUE, "caps", data->videoUdpCaps, NULL);	
		g_signal_connect (data->jitterAppSink, "new-buffer", G_CALLBACK (jitterBuffer), globalDataServer2);

		if (data->mode == Active) {
			g_object_set (data->audioAppSink, "emit-signals", true, "caps", data->audioDecCaps, NULL);	
			g_signal_connect (data->audioAppSink, "new-buffer", G_CALLBACK (newAudioBuffer), globalDataServer2);
		}
	}
	flag_first_or_second_server = 1;
}

void GstClient::buildPipeline(GstData *data) {
	if (data->mode == PASSIVE) {
		gst_bin_add_many (GST_BIN (data->pipeline),
			data->videoUdpSource, data->jitterBuffer, data->jitterTee, data->videoQueue, data->jitterQueue,
			data->jitterAppSink, data->videoRtpDepay, data->videoDecoder, data->videoTee, data->videoDecQueue, 
			data->videoDecAppQueue, data->videoAppSink, data->videoColorspace, data->videoSink,
			NULL);
	}
	else if (data->mode == ACTIVE) {
		gst_bin_add_many (GST_BIN (data->pipeline),
			data->videoUdpSource, data->jitterBuffer, data->jitterTee, data->videoQueue, data->jitterQueue,
			data->jitterAppSink, data->videoRtpDepay, data->videoDecoder, data->videoTee, data->videoDecQueue, 
			data->videoDecAppQueue, data->videoAppSink, data->videoColorspace, data->videoSink, 
			data->audioUdpSource, data->audioRtpDepay, data->audioDecoder, data->audioTee,
			data->audioAppQueue, data->audioAppSink, data->audioQueue, data->audioVolume, data->audioSink, NULL);

		//This is the AUDIO pipeline with tees and sinks	
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
		if (!gst_element_link (data->audioQueue, data->audioVolume)) {
			g_printerr("Couldn't link: audioQueue - audioVolume.\n");
		}
		if (!gst_element_link (data->audioVolume, data->audioSink)) {
			g_printerr("Couldn't link: audioVolume - audioSink.\n");
		}
	}

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
	if(!gst_element_link (data->videoDecQueue, data->videoColorspace)) {
		g_printerr("Couldn't link: videoDecQueue - videoColorSpace.\n");
	}
	if(!gst_element_link (data->videoColorspace, data->videoSink)) {
		g_printerr("Couldn't link: videoColorspace - videoDecoder.\n");
	}
	
}

void GstClient::setPipelineToRun(GstData *data) {
	GstStateChangeReturn ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref (data->pipeline);
	} 
	else {
		g_print("Pipeline playing.\n");	
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

	pthread_exit(NULL);
}

void GstClient::stopAndFreeResources(GstData *data) {
	gst_element_set_state (data->pipeline, GST_STATE_NULL);
	if (data->pipeline) {
		gst_object_unref (data->pipeline);
	}
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

void GstClient::muteAudio(GstData *data) {
    g_object_set (data->audioVolume, "mute", true, NULL);
}

void GstClient::unmuteAudio(GstData *data) {
    g_object_set (data->audioVolume, "mute", false, "volume", 1.0, NULL);
    
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
	gst_element_send_event (data->pipeline, seek_event);
	//gst_element_send_event (data->audioUdpSink, seek_event);
	g_print ("Current playbackRate: %g\n", data->playbackRate);
}

void GstClient::rewindPipeline(GstData *data) {
	if (data->playbackRate > 0) {
		data->playbackRate = -1.0;
	}
	else {
		data->playbackRate *= 2.0;
	}
	sendSeekEvent(data);
	playPipeline(data);
}

void GstClient::fastForwardPipeline(GstData *data) {
	if (data->playbackRate < 0) {
		data->playbackRate = 1.0;
	}
	else {
		data->playbackRate *= 2.0;
	}
	sendSeekEvent(data);
	playPipeline(data);
}
