#include <gst/gst.h>

typedef struct AudioData{
	GstElement * pipeline;
	GstElement * source;
	GstElement * filter;
	GstElement * muxer;
	GstElement * demuxer;
	GstElement * encoder;
	GstElement * decoder;
	GstElement * sink;
	int bitrate;
	GtkWidget * audioRate_entry;
} AudioData;

static void addPadSignalHandler(GstElement * element, GstPad * newPad, AudioData * data);

char * getFileType(char * file);

void recordAudioPipeline(char * source, int rate, int channel, char * format, char * filepath, AudioData * data);

void playAudioPipeline(char * format, char * filepath, int rate, int channel, AudioData * data);

void createAudioPipeline(char * source, int rate, int channel, char * format, char * sink, AudioData * data);

void stopAudioPipeline(AudioData * data);

void audio_start_recording(GtkWidget* source, gpointer* data);

void audio_stop_recording(GtkWidget * source, gpointer * data);

void audio_start_playback(GtkWidget * source, gpointer * data);

void audio_stop_playback(GtkWidget * source, gpointer * data);
