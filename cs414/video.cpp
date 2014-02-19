#include "video.h"

/* 
	Handles the demuxer's new pad signal 
*/
void addPadSignalHandler(GstElement * element, GstPad * newPad, VideoData * data){
	GstPad * decoderPad = gst_element_get_static_pad(data->videoDecoder, "sink");
	GstPadLinkReturn ret;

	g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(newPad), GST_ELEMENT_NAME(element));

	// If our decoder is already linked, we have nothing to do here		
	if(gst_pad_is_linked(decoderPad)){
		g_print("Pad is already linked\n");
		gst_object_unref(decoderPad);
		return;
	}

	ret = gst_pad_link(newPad, decoderPad); 
	if(GST_PAD_LINK_FAILED(ret)){
		g_print("Could not link pads\n");
	} else {
		g_print("Linked demuxer to decoder\n");
	}

	gst_object_unref(decoderPad);
}

/*
	Send a seek event for navigating the video content
	i.e. fast-forwarding, rewinding
*/
void sendSeekEvent(VideoData *data) {
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
		seek_event = gst_event_new_seek(data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0);
	} else {
		seek_event = gst_event_new_seek(data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
	}

	// Send the event
	gst_element_send_event(data->videoSink, seek_event);
	g_print("Current playbackRate: %g\n", data->playbackRate);
}
   
/* Process keyboard input */
gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, VideoData *data) {
  gchar *str = NULL;
   
  if (g_io_channel_read_line(source, &str, NULL ,NULL, NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }
   
  switch (g_ascii_tolower (str[0])) {
  case 'p':
    data->playing = !data->playing;
    gst_element_set_state(data->pipeline, data->playing ? GST_STATE_PLAYING : GST_STATE_PAUSED);
    g_print("Setting state to %s\n", data->playing ? "PLAYING" : "PAUSE");
    break;
  case 's':
    if (g_ascii_isupper (str[0])) {
      data->playbackRate *= 2.0;
    } else {
      data->playbackRate /= 2.0;
    }
    sendSeekEvent(data);
    break;
  case 'd':
    data->playbackRate *= -1.0;
    sendSeekEvent(data);
    break;
  case 'n':   
    gst_element_send_event(data->videoSink, gst_event_new_step(GST_FORMAT_BUFFERS, 1, data->recordingRate, TRUE, FALSE));
    g_print ("Stepping one frame\n");
    break;
  default:
    break;
  }
   
  g_free (str);
  return TRUE;
}

/* 
	Initialize GStreamer components 
*/
void gstreamerSetup(int argc, char *argv[], VideoData *videoData) 
{
	gst_init(&argc, &argv);
  
	// Build Elements 
	videoData->pipeline = NULL;
	videoData->tee = gst_element_factory_make("tee", "tee");
	videoData->fileQueue = gst_element_factory_make("queue", "fileQueue");
	videoData->playerQueue = gst_element_factory_make("queue", "playerQueue");
	videoData->fileSource = gst_element_factory_make("filesrc", "fileSource");
	videoData->fileSink = gst_element_factory_make("filesink", "filesink");

	videoData->muxer = gst_element_factory_make(videoData->muxerPlugin.c_str(), "muxer");
	videoData->demuxer = gst_element_factory_make(videoData->demuxerPlugin.c_str(), "demuxer");
	videoData->videoSource = gst_element_factory_make("dshowvideosrc", "videoSource");
	videoData->videoSink = gst_element_factory_make("dshowvideosink", "videoSink");
	videoData->videoConvert = gst_element_factory_make("autovideoconvert", "videoConvert");
	videoData->videoFilter = gst_element_factory_make("capsfilter", "videoFilter");
	videoData->videoEncoder = gst_element_factory_make(videoData->encoderPlugin.c_str(), "videoEncoder");
	videoData->videoDecoder = gst_element_factory_make(videoData->decoderPlugin.c_str(), "videoDecoder");

	if (videoData->muxer == NULL) g_error("Could not create muxer element");
	if (videoData->demuxer == NULL) g_error("Could not create demuxer element");
	if (videoData->videoEncoder == NULL) g_error("Could not create video encoder element");
	if (videoData->videoDecoder == NULL) g_error("Could not create video decoder element");
	if (videoData->videoSource == NULL) g_error("Could not create video source element");
	if (videoData->videoSink == NULL) g_error("Could not create video sink element");
	if (videoData->videoConvert == NULL) g_error("Could not create video convert element");
	if (videoData->videoFilter == NULL) g_error("Could not create video filter element");

	// Set values 
	g_object_set(videoData->fileSink, "location", videoData->fileSinkPath.c_str(), NULL);
	g_object_set(videoData->fileSource, "location", videoData->fileSourcePath.c_str(), NULL);
	//g_object_set(videoData->playbin, "uri", videoData->fileSourcePath.c_str(), "video-sink", videoData->videoSink, NULL);

	videoData->videoCaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
		"height", G_TYPE_INT, videoData->height,
		"width", G_TYPE_INT, videoData->width,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"framerecordingRate", GST_TYPE_FRACTION, videoData->recordingRate, 1, 
		NULL);
	g_object_set(G_OBJECT(videoData->videoFilter), "caps", videoData->videoCaps, NULL);
	gst_caps_unref (videoData->videoCaps);
}

/* Run the current gstreamer pipeline */
void gstreamerPlay(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PLAYING);
}
/* Pause the current gstreamer pipeline */
void gstreamerPause(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PAUSED);
}

/* Free GStreamer resources */
void gstreamerCleanup(VideoData *videoData)
{
	gst_element_set_state(videoData->pipeline, GST_STATE_NULL);
	gst_object_unref(videoData->pipeline);
}

/* Build the appropriate pipeline from data based on parameter mode */
void gstreamerBuildPipeline(VideoData *videoData, PlayerMode mode)
{
	// Clear previous pipeline if one has already been set up
	if (videoData->pipeline || videoData->pipeline != NULL)
	{
		gst_object_unref(videoData->pipeline);
	}

	videoData->pipeline = gst_pipeline_new("mp1-pipeline");
		
	switch(mode)
	{
		case Camera:
			// Build the camera pipeline
			gst_bin_add_many(GST_BIN(videoData->pipeline), 
				videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, videoData->fileQueue, 
				videoData->videoEncoder, videoData->muxer, videoData->fileSink, videoData->playerQueue, videoData->videoSink, NULL);

			// Link the video recording component: videoSrc -> convert -> filter -> tee
			if (!gst_element_link_many(videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, NULL))
			{
				gst_object_unref(videoData->pipeline);
				g_error("Failed linking: videoSrc -> convert -> filter -> tee \n");
			}
			// Link the media sink pipeline: tee -> queue -> encoder -> muxer -> fileSink
			if (!gst_element_link_many(videoData->tee, videoData->fileQueue, videoData->videoEncoder, videoData->muxer, videoData->fileSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_print("Failed linking: tee -> queue -> encoder -> muxer -> fileSink \n");
			}
			// Link the media player pipeline: tee -> queue -> mediaSink
			if (!gst_element_link_many(videoData->tee, videoData->playerQueue, videoData->videoSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_print("Failed linking: tee -> queue -> mediaSink \n");
			}
			break;

		case File:
			// Build the file playback pipeline
			gst_bin_add_many(GST_BIN(videoData->pipeline), videoData->fileSource, videoData->demuxer, videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL);
			g_signal_connect(videoData->demuxer, "pad-added", G_CALLBACK(addPadSignalHandler), videoData);

			if (!gst_element_link(videoData->fileSource, videoData->demuxer) || !gst_element_link_many(videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_print("Failed linking playback pipeline \n");
			}
			break;
	}
}

/* Sets default video data parameters*/
void initializeVideoData(VideoData * videoData)
{
	videoData->width = 640;
	videoData->height = 480;
	videoData->recordingRate = 20;
	videoData->playbackRate = 1.0;
	videoData->playing = true;
	videoData->encoderPlugin = MJPEG_ENCODER;
	videoData->muxerPlugin = AVI_MUXER;
	videoData->demuxerPlugin = AVI_DEMUXER;
	videoData->fileSinkPath = "video" + string(videoData->encoderPlugin == MJPEG_ENCODER ? ".avi" : ".mp4");
	// NOTE: Absolute user-specific path
	videoData->fileSourcePath = TEST_VIDEO_SOURCE;
	videoData->decoderPlugin = (videoData->fileSourcePath.find(".avi") != string::npos ? MJPEG_DECODER : MPEG4_DECODER); 
}
