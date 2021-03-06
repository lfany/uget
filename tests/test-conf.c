#include <UgApp-base.h>
#include <UgMarkup.h>
#include <UgetData.h>

void test_conf ()
{
	UgMarkup*		markup;
	UgDataset*		dataset;
	UgetProgress*		progress;
	UgetHttp*		http;
	UgetFtp*		ftp;

	markup		= ug_markup_new ();
	dataset		= ug_dataset_new ();
	progress	= ug_dataset_realloc (dataset, UgetProgressInfo, 0);
	http		= ug_dataset_realloc (dataset, UgetHttpInfo, 0);
	ftp			= ug_dataset_realloc (dataset, UgetFtpInfo, 0);

	progress->total = 1484889;
	http->redirection_limit = 10;
	http->referrer	= g_strdup ("http://123.123.123.1/ref");
	ftp->user		= g_strdup ("ftpuser");
	ftp->password	= g_strdup ("ftppassword");

	ug_markup_write_start (markup, "test.ug.xml", TRUE);
	ug_data_write_markup ((UgData*) dataset, markup);
	ug_markup_write_end (markup);

	ug_dataset_unref (dataset);

	dataset = ug_dataset_new ();
	if (ug_markup_parse ("test.ug.xml", &ug_data_parser, dataset)) {
		progress	= ug_dataset_realloc (dataset, UgetProgressInfo, 0);
		http		= ug_dataset_realloc (dataset, UgetHttpInfo, 0);
		ftp			= ug_dataset_realloc (dataset, UgetFtpInfo, 0);
	}

	ug_dataset_unref (dataset);
	ug_markup_free (markup);
}

int main (int argc, char* argv[])
{
	uglib_init ();

	test_conf ();

	uglib_finalize ();

	return 0;
}

