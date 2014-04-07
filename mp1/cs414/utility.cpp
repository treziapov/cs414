#include "utility.h"

string integerToString(int i)
{
	ostringstream convert;
	convert << i;
	return convert.str();
}

int stringToInteger(string s)
{
	int result;
	istringstream convert(s);
	if (!(convert >> result)) result = 0;
	return result;
}

int charPointerToInteger(const char* c)
{
	string s(c);
	return stringToInteger(s);
}

string gtkGetUserSaveFile()
{
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new("Save File",
	   				      NULL,
	   				      GTK_FILE_CHOOSER_ACTION_SAVE,
	   				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	   				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	   				      NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "C:/");
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Untitled");

	gtk_dialog_run(GTK_DIALOG (dialog));
	char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	gtk_widget_destroy (dialog);
	return string(filename);
}

string gtkGetUserFile()
{
	GtkWidget *filechooserdialog = gtk_file_chooser_dialog_new("FileChooserDialog", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gtk_dialog_run(GTK_DIALOG(filechooserdialog));
	char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooserdialog));	
	gtk_widget_destroy(filechooserdialog);
	return string(filename);
}