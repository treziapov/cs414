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

void gstreamerBuildElementsOnce(VideoData *videoData)
{

}

/* 
	Initialize GStreamer components 
*/
void gstreamerBuildElements(VideoData *videoData) 
{ 
	// Build Elements 
	videoData->pipeline = gst_pipeline_new("pipeline");
	videoData->videoSource = gst_element_factory_make("dshowvideosrc", "videoSource");
	videoData->videoSink = gst_element_factory_make("dshowvideosink", "videoSink");
	videoData->tee = gst_element_factory_make("tee", "tee");
	videoData->fileQueue = gst_element_factory_make("queue", "fileQueue");
	videoData->playerQueue = gst_element_factory_make("queue", "playerQueue");
	videoData->fileSource = gst_element_factory_make("filesrc", "fileSource");
	videoData->fileSink = gst_element_factory_make("filesink", "filesink");
	videoData->videoConvert = gst_element_factory_make("autovideoconvert", "videoConvert");
	videoData->videoFilter = gst_element_factory_make("capsfilter", "videoFilter");

	if (videoData->videoSource == NULL) g_error("Could not create video source element");
	if (videoData->videoSink == NULL) g_error("Could not create video sink element");
	if (videoData->videoConvert == NULL) g_error("Could not create video convert element");
	if (videoData->videoFilter == NULL) g_error("Could not create video filter element");

	// Set values 
	g_object_set(videoData->fileSink, "location", videoData->fileSinkPath.c_str(), NULL);
	g_object_set(videoData->fileSource, "location", videoData->fileSourcePath.c_str(), NULL);

	if (videoData->fileSourcePath.find(".avi") != string::npos)
	{
		videoData->decoderPlugin = MJPEG_DECODER;
		videoData->demuxerPlugin = AVI_DEMUXER;
	}
	else
	{
		videoData->decoderPlugin = MPEG4_DECODER;
		videoData->demuxerPlugin = QT_DEMUXER;
	}

	videoData->muxerPlugin = AVI_MUXER;
	videoData->muxer = gst_element_factory_make(videoData->muxerPlugin.c_str(), "muxer");
	videoData->demuxer = gst_element_factory_make(videoData->demuxerPlugin.c_str(), "demuxer");
	videoData->videoDecoder = gst_element_factory_make(videoData->decoderPlugin.c_str(), "videoDecoder");
	videoData->videoEncoder = gst_element_factory_make(videoData->encoderPlugin.c_str(), "videoEncoder");

	if (videoData->muxer == NULL) g_error("Could not create muxer element");
	if (videoData->demuxer == NULL) g_error("Could not create demuxer element");
	if (videoData->videoEncoder == NULL) g_error("Could not create video encoder element");
	if (videoData->videoDecoder == NULL) g_error("Could not create video decoder element");

	videoData->videoCaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
		"height", G_TYPE_INT, videoData->height,
		"width", G_TYPE_INT, videoData->width,
		//"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"framererate", GST_TYPE_FRACTION, videoData->recordingRate, 1, 
		NULL);
	g_object_set(G_OBJECT(videoData->videoFilter), "caps", videoData->videoCaps, NULL);
	gst_caps_unref (videoData->videoCaps);

	if (videoData->windowDrawn)
	{
		setVideoWindow_event(videoData->window, videoData);
	}
}

/* Run the current gstreamer pipeline */
void gstreamerPlay(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PLAYING);
	videoData->state = GST_STATE_PLAYING;
}

/* Pause the current gstreamer pipeline */
void gstreamerPause(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PAUSED);
	videoData->state = GST_STATE_PAUSED;
}

/* Stop the current gstreamer pipeline */
void gstreamerStop(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_READY);
	videoData->state = GST_STATE_READY;
}

/* Free GStreamer resources */
void gstreamerCleanup(VideoData *videoData)
{
	gst_element_set_state(videoData->pipeline, GST_STATE_NULL);
	videoData->state = GST_STATE_NULL;
	gst_object_unref(videoData->pipeline);
}

/* 
	This function is called when the GUI toolkit creates the physical mainWindow that will hold the video.
	At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
	and pass it to GStreamer through the XOverlay interface.
*/
void setVideoWindow_event(GtkWidget *widget, VideoData *data) 
{
	GdkWindow *mainWindow = gtk_widget_get_window(widget);
	guintptr window_handle;
	
	if (!gdk_window_ensure_native(mainWindow))
	{
		g_error("Couldn't create native mainWindow needed for GstXOverlay!");
	}	

	// Retrieve mainWindow handler from GDK
	window_handle = (guintptr)GDK_WINDOW_HWND(mainWindow);

	// Pass it to playbin2, which implements XOverlay and will forward it to the video sink
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(data->videoSink), window_handle);

	data->windowDrawn = true;
}

/* Build the appropriate pipeline from data based on parameter mode */
void gstreamerBuildPipeline(VideoData *videoData, PlayerMode mode)
{
	// Unlink elements depending on current mode
	switch(videoData->playerMode)
	{
		case Camera:
			gst_element_set_state (videoData->pipeline, GST_STATE_NULL); 
			gst_element_unlink_many(videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, NULL);
			gst_element_unlink_many(videoData->tee, videoData->fileQueue, videoData->videoEncoder, videoData->muxer, videoData->fileSink, NULL);
			gst_element_unlink_many(videoData->tee, videoData->playerQueue, videoData->videoSink, NULL);
			gst_bin_remove_many(GST_BIN(videoData->pipeline), 
				videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, videoData->fileQueue, 
				videoData->videoEncoder, videoData->muxer, videoData->fileSink, videoData->playerQueue, videoData->videoSink, NULL);		
			break;
		case File:
			gst_element_set_state (videoData->pipeline, GST_STATE_NULL);
			gst_element_unlink(videoData->fileSource, videoData->demuxer);
			gst_element_unlink_many(videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL);
			gst_bin_remove_many(GST_BIN(videoData->pipeline), videoData->fileSource, videoData->demuxer, videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL);
			break;
		default: 
			break;
	}

	gstreamerBuildElements(videoData);

	switch(mode)
	{
		case Camera:
			if (videoData->encoderPlugin == MJPEG_ENCODER)
			{
				// Build the camera pipeline
				gst_bin_add_many(GST_BIN(videoData->pipeline), 
					videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, videoData->fileQueue, 
					videoData->videoEncoder, videoData->fileSink, videoData->playerQueue, videoData->muxer, videoData->videoSink, NULL);

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
					g_error("Failed linking: tee -> queue -> mediaSink \n");
				}
			}
			else if (videoData->encoderPlugin == MPEG4_ENCODER)
			{
				// Build the camera pipeline
				gst_bin_add_many(GST_BIN(videoData->pipeline), 
					videoData->videoSource, videoData->videoFilter, videoData->videoConvert, videoData->tee, videoData->playerQueue, videoData->videoSink,
					videoData->fileQueue, videoData->videoEncoder, videoData->muxer, videoData->fileSink, NULL);
				// Link the video recording component: videoSrc -> convert -> filter -> tee
				if (!gst_element_link_many(videoData->videoSource, videoData->videoFilter, videoData->videoConvert, videoData->tee, NULL))
				{
					g_print("Failed linking: videoSrc -> convert -> filter -> tee \n");
				}
				// Link the media sink pipeline: tee -> queue -> encoder -> muxer -> fileSink
				if (!gst_element_link_many(videoData->tee, videoData->fileQueue, videoData->videoEncoder, videoData->muxer, videoData->fileSink, NULL)) 
				{
					g_print("Failed linking: tee -> queue -> encoder -> muxer -> fileSink \n");
				}
				// Link the media player pipeline: tee -> queue -> mediaSink
				if (!gst_element_link_many(videoData->tee, videoData->playerQueue, videoData->videoSink, NULL)) 
				{
					g_print("Failed linking: tee -> queue -> mediaSink \n");
				}
			}
			break;

		case File:
			// Build the file playback pipeline
			gst_bin_add_many(GST_BIN(videoData->pipeline), videoData->fileSource, videoData->demuxer, videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL);
			g_signal_connect(videoData->demuxer, "pad-added", G_CALLBACK(addPadSignalHandler), videoData);

			if (!gst_element_link(videoData->fileSource, videoData->demuxer) || !gst_element_link_many(videoData->videoDecoder, videoData->videoConvert, videoData->videoSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_error("Failed linking playback pipeline \n");
			}
			break;
	}

	gst_element_set_state(videoData->pipeline, GST_STATE_PLAYING);
	videoData->state = GST_STATE_PLAYING;
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
	videoData->duration = GST_CLOCK_TIME_NONE;
	videoData->playerMode = Initial;
	videoData->state = GST_STATE_NULL;
	videoData->windowDrawn = false;
}

