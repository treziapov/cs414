#include <gst/gst.h>
#include <iostream>
#include <string>

using namespace std;

typedef struct _GstData {
	GstElement *pipeline;
	GstElement *videosrc, *videosink;
	GstElement *colorSpaceFilter;
	GstElement *filter;
	GstCaps *videoCaps;
	GstElement *tee, *queue;
	GstElement fileSink;
} GstData;

typedef struct _VideoSettings {
	string source;
	int height;
	int width;
	int rate;
	string encoder;
} VideoSettings;

int main(int argc, char *argv[]) {
  GstData data;
  GstBus *bus;
  GstMessage *msg;
  
  /* Initialize GStreamer */
  gst_init (&argc, &argv);
  
  data.videosrc = gst_element_factory_make("ksvideosrc", "videosrc");
  data.videosink = gst_element_factory_make("autovideosink", "videosink");

  /* Build the pipeline */
  data.pipeline = gst_parse_launch ("ksvideosrc ! video/x-raw-yuv,width=640,height=480 ! ffmpegcolorspace ! tee name=my_videosink ! jpegenc ! avimux ! filesink location=video.avi my_videosink. ! queue ! autovideosink", NULL);

  /* Start playing */
  gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  
  /* Wait until error or EOS */
  bus = gst_element_get_bus (data.pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  
  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  return 0;
}