/*
 *
 *   Copyright (C) 2005-2014 by C.H. Huang
 *   plushuang.tw@gmail.com
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU Lesser General Public License in all respects
 *  for all of the code used other than OpenSSL.  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so.  If you
 *  do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <UgUri.h>
#include <UgUtils.h>
#include <UgString.h>
#include <UgRegistry.h>
#include <UgCategory.h>
#include <UgetData.h>
#include <UgPlugin.h>


// ----------------------------------------------------------------------------
// UgCategory

static void	ug_category_assign   (UgCategory* category, UgCategory* src);
// UgCategory.indices parse/write for UgMarkup
static void	ug_int_list_in_markup (GList** list, GMarkupParseContext* context);
static void	ug_int_list_to_markup (GList** list, UgMarkup* markup);

static const UgDataEntry	ug_category_entry[] =
{
	{"name",			G_STRUCT_OFFSET (UgCategory, name),				UG_TYPE_STRING,		NULL,	NULL},
	{"ActiveLimit",		G_STRUCT_OFFSET (UgCategory, active_limit),		UG_TYPE_UINT,		NULL,	NULL},
	{"FinishedLimit",	G_STRUCT_OFFSET (UgCategory, finished_limit),	UG_TYPE_UINT,		NULL,	NULL},
	{"RecycledLimit",	G_STRUCT_OFFSET (UgCategory, recycled_limit),	UG_TYPE_UINT,		NULL,	NULL},
	{"DownloadDefault",	G_STRUCT_OFFSET (UgCategory, defaults),			UG_TYPE_INSTANCE,	&ug_dataset_iface,	NULL},
	{"DownloadIndices",	G_STRUCT_OFFSET (UgCategory, indices),			UG_TYPE_CUSTOM,		ug_int_list_in_markup,	ug_int_list_to_markup},
	{NULL},			// null-terminated
};
// extern
const UgDataInterface	ug_category_iface =
{
	sizeof (UgCategory),	// instance_size
	"category",				// name
	ug_category_entry,		// entry

	(UgInitFunc)     ug_category_init,
	(UgFinalizeFunc) ug_category_finalize,
	(UgAssignFunc)   ug_category_assign,
};
// extern
const UgDataInterface*	ug_category_iface_pointer = &ug_category_iface;


void	ug_category_init (UgCategory* category)
{
//	category->iface = ug_category_iface_pointer;

	category->defaults = ug_dataset_new ();
	category->active_limit   = 3;
	category->finished_limit = 300;
	category->recycled_limit = 300;
}

void	ug_category_finalize (UgCategory* category)
{
	if (category->destroy.func)
		category->destroy.func (category->destroy.data);

	g_free (category->name);
	if (category->defaults)
		ug_dataset_unref (category->defaults);
}

static void	ug_category_assign (UgCategory* category, UgCategory* src)
{
	ug_str_set (&category->name, src->name, -1);

	category->active_limit   = src->active_limit;
	category->finished_limit = src->finished_limit;
	category->recycled_limit = src->recycled_limit;

	ug_data_assign (category->defaults, src->defaults);
}

UgCategory*	ug_category_new (void)
{
	return ug_data_new (ug_category_iface_pointer);
}

void	ug_category_free (UgCategory* category)
{
	ug_data_free (category);
}

// add dataset to category and increase reference count of dataset.
void	ug_category_add (UgCategory* category, UgDataset* dataset)
{
	UgetLog*			datalog;

	// added on
	datalog = ug_dataset_realloc (dataset, UgetLogInfo, 0);
	if (datalog->added_on == NULL)
		datalog->added_on = ug_str_from_time (time (NULL), FALSE);

	category->funcs->add (category, dataset);
}

// get all tasks(UgDataset) in this category.
// To free the returned value, use g_list_free (list).
GList*	ug_category_get_all (UgCategory* category)
{
	return category->funcs->get_all (category);
}

// get queuing tasks(UgDataset) in this category/
// This function should be noticed UgCategory::active_limit, because
// application will try to activate all returned dataset.
// To free the returned value, use g_list_free (list).
GList*	ug_category_get_tasks (UgCategory* category)
{
	return category->funcs->get_tasks (category);
}

// used to notify category that it's dataset was changed.
// It may change hints and switch dataset to another internal queue of category.
void	ug_category_changed  (UgCategory* category, UgDataset* dataset)
{
	category->funcs->changed (category, dataset);
}


// ----------------------------------------------------------------------------
// Category.indices load/save
//
static void ug_int_list_start_element (GMarkupParseContext*	context,
                                       const gchar*			element_name,
                                       const gchar**		attr_names,
                                       const gchar**		attr_values,
                                       GList**				list,
                                       GError**				error)
{
	guint	index;
	int		value;

	for (index=0; attr_names[index]; index++) {
		if (strcmp (attr_names[index], "value") != 0)
			continue;
		value = atoi (attr_values[index]);
		*list = g_list_prepend (*list, GINT_TO_POINTER (value));
	}

	g_markup_parse_context_push (context, &ug_markup_skip_parser, NULL);
}

static GMarkupParser	ug_int_list_parser =
{
	(gpointer) ug_int_list_start_element,
	(gpointer) g_markup_parse_context_pop,
	NULL,
	NULL,
	NULL
};

static void	ug_int_list_in_markup (GList** list, GMarkupParseContext* context)
{
	g_markup_parse_context_push (context, &ug_int_list_parser, list);
}

static void	ug_int_list_to_markup (GList** list, UgMarkup* markup)
{
	GList*		link;
	guint		value;

	for (link = g_list_last (*list);  link;  link = link->prev) {
		value = GPOINTER_TO_INT (link->data);
		ug_markup_write_element_start (markup, "int value='%d'", value);
		ug_markup_write_element_end   (markup, "int");
	}
}


// ----------------------------------------------------------------------------
// CategoryList load/save
//
static void ug_category_data_start_element (GMarkupParseContext*	context,
                                            const gchar*		element_name,
                                            const gchar**		attr_names,
                                            const gchar**		attr_values,
                                            GList**				list,
                                            GError**			error)
{
	UgCategory*		category;

	if (strcmp (element_name, "category") != 0) {
		g_markup_parse_context_push (context, &ug_markup_skip_parser, NULL);
		return;
	}

	// user must register data interface of UgCategory.
	category = ug_data_new (ug_category_iface_pointer);
	*list = g_list_prepend (*list, category);
	g_markup_parse_context_push (context, &ug_data_parser, category);
}

static GMarkupParser	ug_category_data_parser =
{
	(gpointer) ug_category_data_start_element,
	(gpointer) g_markup_parse_context_pop,
	NULL,
	NULL,
	NULL
};

static void ug_category_list_start_element (GMarkupParseContext*	context,
                                            const gchar*		element_name,
                                            const gchar**		attr_names,
                                            const gchar**		attr_values,
                                            GList**				list,
                                            GError**			error)
{
//	guint	index;

//	if (strcmp (element_name, "UgCategoryList") == 0) {
//		for (index=0; attr_names[index]; index++) {
//			if (strcmp (attr_names[index], "version") != 0)
//				continue;
//			if (strcmp (attr_values[index], "1") == 0) {
				g_markup_parse_context_push (context, &ug_category_data_parser, list);
				return;
//			}
//			// others...
//			break;
//		}
//	}

//	g_markup_parse_context_push (context, &ug_markup_skip_parser, NULL);
}

static GMarkupParser	ug_category_list_parser =
{
	(gpointer) ug_category_list_start_element,
	(gpointer) g_markup_parse_context_pop,
	NULL,
	NULL,
	NULL
};

GList*	ug_category_list_load (const gchar* file)
{
	GList*		category_list;

	category_list = NULL;
	ug_markup_parse (file, &ug_category_list_parser, &category_list);
	return category_list;
}

gboolean	ug_category_list_save (GList* list, const gchar* file)
{
	UgCategoryGetAllFunc	get_all;
	UgCategory*	category;
	UgMarkup*	markup;
	GList*		link;
	guint		index;

	markup = ug_markup_new ();
	if (ug_markup_write_start (markup, file, TRUE) == FALSE) {
		ug_markup_free (markup);
		return FALSE;
	}

	ug_markup_write_element_start (markup, "UgCategoryList version='1'");
	for (list = g_list_last (list);  list;  list = list->prev) {
		category = list->data;
		get_all = category->funcs->get_all;
		// create UgCategory.indices
		category->indices = get_all (category);
		for (link = category->indices;  link;  link = link->next) {
			index = UG_DATASET_RELATION ((UgDataset*) link->data)->index;
			link->data = GINT_TO_POINTER (index);
		}
		// output
		ug_markup_write_element_start (markup, "category");
		ug_data_write_markup ((UgData*) list->data, markup);
		ug_markup_write_element_end (markup, "category");
		// free UgCategory.indices
		g_list_free (category->indices);
		category->indices = NULL;
	}
	ug_markup_write_element_end (markup, "UgCategoryList");

	ug_markup_write_end (markup);
	return	TRUE;
}

void	ug_category_list_link (GList* list, GList* download_list)
{
	UgCategoryAddFunc	add_func;
	GPtrArray*	array;
	UgCategory*	category;
	GList*		link;
	guint		index;

	// create array from download_list
	array = g_ptr_array_sized_new (g_list_length (download_list));
	for (link = download_list;  link;  link = link->next)
		array->pdata[array->len++] = link->data;

	// link tasks in category
	for (;  list;  list = list->next) {
		category = list->data;
		add_func = category->funcs->add;
		// get tasks from array by index
		for (link = category->indices;  link;  link = link->next) {
			index = GPOINTER_TO_INT (link->data);
			if (index < array->len)
				add_func (category, g_ptr_array_index (array, index));
		}
		// free list
		g_list_free (category->indices);
		category->indices = NULL;
	}

	// free array
	g_ptr_array_free (array, TRUE);
}


// ----------------------------------------------------------------------------
// DownloadList load/save
//
static void ug_download_data_start_element (GMarkupParseContext*	context,
                                            const gchar*		element_name,
                                            const gchar**		attr_names,
                                            const gchar**		attr_values,
                                            GList**				list,
                                            GError**			error)
{
	UgDataset*		dataset;

	if (strcmp (element_name, "download") != 0) {
		g_markup_parse_context_push (context, &ug_markup_skip_parser, NULL);
		return;
	}

	dataset = ug_dataset_new ();
	*list = g_list_prepend (*list, dataset);
	g_markup_parse_context_push (context, &ug_data_parser, dataset);
}

static GMarkupParser	ug_download_data_parser =
{
	(gpointer) ug_download_data_start_element,
	(gpointer) g_markup_parse_context_pop,
	NULL,
	NULL,
	NULL
};

static void ug_download_list_start_element (GMarkupParseContext*	context,
                                            const gchar*		element_name,
                                            const gchar**		attr_names,
                                            const gchar**		attr_values,
                                            GList**				list,
                                            GError**			error)
{
//	guint	index;

//	if (strcmp (element_name, "UgDownloadList") == 0) {
//		for (index=0; attr_names[index]; index++) {
//			if (strcmp (attr_names[index], "version") != 0)
//				continue;
//			if (strcmp (attr_values[index], "1") == 0) {
				g_markup_parse_context_push (context, &ug_download_data_parser, list);
				return;
//			}
			// others...
//			break;
//		}
//	}

//	g_markup_parse_context_push (context, &ug_markup_skip_parser, NULL);
}

static GMarkupParser	ug_download_list_parser =
{
	(gpointer) ug_download_list_start_element,
	(gpointer) g_markup_parse_context_pop,
	NULL,
	NULL,
	NULL
};

GList*	ug_download_list_load (const gchar* download_file)
{
	UgetRelation*		relation;
	GList*			list;
	GList*			link;

	list = NULL;
	ug_markup_parse (download_file, &ug_download_list_parser, &list);
	// attachment
	for (link = list;  link;  link = link->next) {
		relation = UG_DATASET_RELATION ((UgDataset*) link->data);
		ug_attachment_ref (relation->attached.stamp);
	}

	return list;
}

gboolean	ug_download_list_save (GList* list, const gchar* download_file)
{
	UgetRelation*	relation;
	UgMarkup*	markup;
	guint		index;

	markup = ug_markup_new ();
	if (ug_markup_write_start (markup, download_file, TRUE) == FALSE) {
		ug_markup_free (markup);
		return FALSE;
	}

	for (index = 0;  list;  list = list->next, index++) {
		relation = UG_DATASET_RELATION ((UgDataset*) list->data);
		if (relation)
			relation->index = index;
		if (list->next == NULL)
			break;
	}

	ug_markup_write_element_start (markup, "UgDownloadList version='1'");
	for (;  list;  list = list->prev) {
		ug_markup_write_element_start (markup, "download");
		ug_data_write_markup ((UgData*) list->data, markup);
		ug_markup_write_element_end (markup, "download");
	}
	ug_markup_write_element_end (markup, "UgDownloadList");

	ug_markup_write_end (markup);
	return	TRUE;
}

// ----------------------------------------------------------------------------
// Below utility functions can be used by g_list_foreach()

gboolean	ug_download_create_attachment (UgDataset* dataset, gboolean force)
{
	UgetHttp*		http;
	UgetRelation*		relation;
	gchar*			file;
	gchar*			dir;
	guint			dir_len;

	// check
	relation = UG_DATASET_RELATION (dataset);
	if (relation == NULL)
		relation = ug_dataset_alloc_front (dataset, UgetRelationInfo);
	else if (relation->attached.stamp)
		return FALSE;
	http = ug_dataset_get (dataset, UgetHttpInfo, 0);
	if ( (http && (http->cookie_file || http->post_file)) == FALSE && force == FALSE)
		return FALSE;

	// create attachment folder
	dir = ug_attachment_alloc (&relation->attached.stamp);
	if (dir == NULL)
		return FALSE;
	dir_len = strlen (dir);
	g_free (relation->attached.folder);
	relation->attached.folder = dir;

	// UgetHttp
	if (http) {
		if (http->cookie_file && dir_len != strspn (dir, http->cookie_file))
		{
			file = g_build_filename (dir, "http-CookieFile", NULL);
			// copy file and save path
			if (ug_copy_file (http->cookie_file, file) == -1)
				g_free (file);
			else {
				g_free (http->cookie_file);
				http->cookie_file = file;
			}
		}
		if (http->post_file && dir_len != strspn (dir, http->post_file))
		{
			file = g_build_filename (dir, "http-PostFile", NULL);
			// copy file and save path
			if (ug_copy_file (http->post_file, file) == -1)
				g_free (file);
			else {
				g_free (http->post_file);
				http->post_file = file;
			}
		}
	}

	return TRUE;
}

gboolean	ug_download_assign_attachment (UgDataset* dest_data, UgDataset* src_data)
{
	union {
		UgetHttp*		http;
		UgetRelation*		relation;
	} src;
	union {
		UgetHttp*		http;
		UgetRelation*		relation;
	} dest;

	// relation
	src.relation = UG_DATASET_RELATION (src_data);
	if (src.relation == NULL || src.relation->attached.stamp == 0)
		return FALSE;
	dest.relation = UG_DATASET_RELATION (dest_data);
	if (dest.relation == NULL)
		dest.relation = ug_dataset_alloc_front (src_data, UgetRelationInfo);
	ug_attachment_ref (src.relation->attached.stamp);
	ug_attachment_unref (dest.relation->attached.stamp);
	dest.relation->attached.stamp = src.relation->attached.stamp;
	ug_str_set (&dest.relation->attached.folder, src.relation->attached.folder, -1);

	// http
	src.http  = ug_dataset_get (src_data, UgetHttpInfo, 0);
	if (src.http) {
		dest.http = ug_dataset_realloc (dest_data, UgetHttpInfo, 0);
		ug_str_set (&dest.http->post_file,   src.http->post_file,   -1);
		ug_str_set (&dest.http->cookie_file, src.http->cookie_file, -1);
	}
	return TRUE;
}

void	ug_download_delete_temp (UgDataset* dataset)
{
	UgetCommon*	common;
	gchar*			file;
	gchar*			file_aria2;
	gchar*			path;

	common = UG_DATASET_COMMON (dataset);
	if (common && common->file) {
		file = g_strconcat (common->file, ".ug_", NULL);
		file_aria2 = g_strconcat (common->file, ".aria2", NULL);
		if (common->folder == NULL) {
			ug_delete_file (file);
			ug_delete_file (file_aria2);
		}
		else {
			path = g_build_filename (common->folder, file, NULL);
			ug_delete_file (path);
			g_free (path);
			// aria2 control file
			path = g_build_filename (common->folder, file_aria2, NULL);
			ug_delete_file (path);
			g_free (path);
		}
		g_free (file);
	}
}

void	ug_download_complete_data (UgDataset* dataset)
{
	UgUri         uripart;
	UgetCommon*   common;
	const gchar*  string;
	guint         length;

	common = UG_DATASET_COMMON (dataset);
	if (common == NULL)
		return;
	if (ug_uri_init (&uripart, common->url) == 0)
		return;
	// file
//	if (common->file == NULL) {
//		string = ug_uri_get_file (&uripart);
//		if (string)
//			common->file = (gchar*) string;
//		else
//			common->file = g_strdup ("index.htm");
//	}
	// user
	if (common->user == NULL) {
		length = ug_uri_part_user (&uripart, &string);
		if (length)
			common->password = g_strndup (string, length);
	}
	// password
	if (common->password == NULL) {
		length = ug_uri_part_password (&uripart, &string);
		if (length)
			common->password = g_strndup (string, length);
	}
	// Remove user & password from URL
	if (uripart.authority != uripart.host) {
		memmove ((char*)uripart.uri + uripart.authority, uripart.uri + uripart.host,
				strlen (uripart.uri + uripart.host) + 1);
	}
}


// ----------------------------------------------------------------------------
// UgetRelation : relation of UgCategory, UgDataset, and UgPlugin.
//
static void	uget_relation_final  (UgetRelation* relation);
static void	uget_relation_assign (UgetRelation* relation, UgetRelation* src);

static const UgDataEntry	uget_relation_entry[] =
{
	{"hints",			G_STRUCT_OFFSET (UgetRelation, hints),			UG_TYPE_UINT,	NULL,	NULL},
	{"AttachedFolder",	G_STRUCT_OFFSET (UgetRelation, attached.folder),	UG_TYPE_STRING,	NULL,	NULL},
	{"AttachedStamp",	G_STRUCT_OFFSET (UgetRelation, attached.stamp),	UG_TYPE_UINT,	NULL,	NULL},
	{"MessageType",		G_STRUCT_OFFSET (UgetRelation, message.type),		UG_TYPE_UINT,	NULL,	NULL},
	{"MessageString",	G_STRUCT_OFFSET (UgetRelation, message.string),	UG_TYPE_STRING,	NULL,	NULL},
	{NULL},			// null-terminated
};
// extern
const UgDataInterface	uget_relation_iface =
{
	sizeof (UgetRelation),	// instance_size
	"relation",				// name
	uget_relation_entry,		// entry

	(UgInitFunc)     NULL,
	(UgFinalizeFunc) uget_relation_final,
	(UgAssignFunc)   uget_relation_assign,
};
// extern
const UgDataInterface*	uget_relation_iface_pointer = &uget_relation_iface;


static void	uget_relation_final (UgetRelation* relation)
{
	if (relation->destroy.func)
		relation->destroy.func (relation->destroy.data);

	g_free (relation->attached.folder);
	ug_attachment_unref (relation->attached.stamp);
}

static void	uget_relation_assign (UgetRelation* relation, UgetRelation* src)
{
	// hints
	// used by UgDownloadDialog
	if (src->hints & UG_HINT_PAUSED) {
		relation->hints |=  UG_HINT_PAUSED;
		relation->hints &= ~UG_HINT_ACTIVE;
	}
	else
		relation->hints &= ~UG_HINT_PAUSED;

	// attachment
	if (relation->attached.stamp != src->attached.stamp) {
		ug_attachment_ref (src->attached.stamp);
		ug_attachment_unref (relation->attached.stamp);
		relation->attached.stamp = src->attached.stamp;
		ug_str_set (&relation->attached.folder, src->attached.folder, -1);
	}
}

