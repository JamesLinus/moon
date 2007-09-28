/*
 * moon-plugin.cpp: MoonLight browser plugin.
 *
 * Author:
 *   Everaldo Canuto (everaldo@novell.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#include "plugin.h"
#include "plugin-class.h"
#include "moon-mono.h"
#include "downloader.h"

/* gleaned from svn log of the moon module, as well as olive/class/{agclr,agmono,System.Silverlight} */
static const gchar *moonlight_authors[] = {
	"Andreia Gaita <avidigal@novell.com>",
	"Atsushi Enomoto <atsushi@ximian.com>",
	"Chris Toshok <toshok@ximian.com>",
	"Dick Porter <dick@ximian.com>",
	"Everaldo Canuto <ecanuto@novell.com>",
	"Jackson Harper <jackson@ximian.com>",
	"Jeffrey Stedfast <fejj@novell.com>",
	"Larry Ewing <lewing@novell.com>",
	"Marek Habersack <mhabersack@novell.com>",
	"Miguel de Icaza <miguel@novell.com>",
	"Rodrigo Kumpera <rkumpera@novell.com>",
	"Rolf Bjarne Kvinge <RKvinge@novell.com>",
	"Sebastien Pouliot <sebastien@ximian.com>",
	"Jb Evain <jbevain@novell.com>",
	NULL
};

void
plugin_menu_about (PluginInstance *plugin)
{
	GtkAboutDialog *about = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());

	gtk_about_dialog_set_name (about, PLUGIN_OURNAME);
	gtk_about_dialog_set_version (about, PLUGIN_OURVERSION);

	gtk_about_dialog_set_copyright (about, "Copyright 2007 Novell, Inc. (http://www.novell.com/)");
	gtk_about_dialog_set_website (about, "http://mono-project.com/Moonlight/");
	gtk_about_dialog_set_website_label (about, "Project Website");

	gtk_about_dialog_set_authors (about, moonlight_authors);

	/* Newer gtk+ versions require this for the close button to work */
	g_signal_connect_swapped (about,
							  "response", 
							  G_CALLBACK (gtk_widget_destroy),
							  about);

	gtk_dialog_run (GTK_DIALOG (about));
}

void
plugin_show_menu (PluginInstance *plugin)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	char *name;

	menu = gtk_menu_new();
	
	name = g_strdup_printf ("%s %s", PLUGIN_OURNAME, PLUGIN_OURVERSION);
	menu_item = gtk_menu_item_new_with_label (name);
	g_free (name);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	g_signal_connect_swapped (G_OBJECT(menu_item), "activate", G_CALLBACK (plugin_menu_about), plugin);

	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

gboolean
plugin_event_callback (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gboolean handled = 0;

	PluginInstance *plugin = (PluginInstance *) user_data;
	GdkEventButton *event_button;

	switch (event->type) {

		case GDK_BUTTON_PRESS:
			event_button = (GdkEventButton *) event;
			if (event_button->button == 3) {
				plugin_show_menu (plugin);
			}
			handled = 1;
			break;

		default:
			break;
	}

	return handled;
}

/*** PluginInstance:: *********************************************************/

GSList *plugin_instances = NULL;

void
plugin_set_unload_callback (PluginInstance* plugin, plugin_unload_callback* puc)
{
	if (!plugin) {
		printf ("Trying to set plugin unload callback on a null plugin.\n");
		return;
	}
	
	plugin->SetUnloadCallback (puc);
}

PluginInstance::PluginInstance (NPP instance, uint16 mode)
{
	this->mode = mode;
	this->instance = instance;
	this->window = NULL;

	this->rootobject = NULL;

	this->container = NULL;
	this->surface = NULL;

	// Property fields
	this->initParams = false;
	this->isLoaded = false;
	this->source = NULL;
	this->onLoad = NULL;
	this->onError = NULL;
	this->background = NULL;

	this->windowless = false;
	
	this->vm_missing_file = NULL;
	this->xaml_loader = NULL;
	this->plugin_unload = NULL;

	this->timers = NULL;

	plugin_instances = g_slist_append (plugin_instances, this->instance);

	/* back pointer to us */
	instance->pdata = this;
}

PluginInstance::~PluginInstance ()
{
	// Kill timers
	GSList *p;
	for (p = timers; p != NULL; p = p->next){
		uint32_t source_id = GPOINTER_TO_INT (p->data);

		g_source_remove (source_id);
	}
	g_slist_free (p);
	
	// Remove us from the list.
	plugin_instances = g_slist_remove (plugin_instances, this->instance);

	// 
	// The code below was an attempt at fixing this, but we are still getting spurious errors
	// we might have another source of problems
	//
	fprintf (stderr, "Destroying the surface: %p, plugin: %p\n", surface, this);
	if (surface != NULL){
		//gdk_error_trap_push ();
		surface->unref ();
		//gdk_display_sync (this->display);
		//gdk_error_trap_pop ();
	}

	if (rootobject)
		NPN_ReleaseObject ((NPObject*)rootobject);

	if (background)
		g_free (background);

	delete xaml_loader;
	xaml_loader = NULL;
	
	if (plugin_unload)
		plugin_unload (this);
}

void
PluginInstance::SetUnloadCallback (plugin_unload_callback* puc)
{
	plugin_unload = puc;
}

void 
PluginInstance::Initialize (int argc, char* const argn[], char* const argv[])
{
	for (int i = 0; i < argc; i++) {
		if (argn[i] == NULL)
			continue;

		// initParams.
		if (!strcasecmp (argn[i], "initParams")) {
			this->initParams = argv[i];
		}

		// onLoad.
		if (!strcasecmp (argn[i], "onLoad")) {
			this->onLoad = argv[i];
		}

		// onError.
		if (!strcasecmp (argn[i], "onError")) {
			this->onError = argv[i];
		}

		// Source url handle.
		if (!strcasecmp (argn[i], "src") || !strcasecmp (argn[i], "source")) {
			this->source = argv[i];
		}

		if (!strcasecmp (argn[i], "background")) {
			this->background = g_strdup (argv[i]);
		}
	}
}

void 
PluginInstance::Finalize ()
{
}

NPError 
PluginInstance::GetValue (NPPVariable variable, void *result)
{
	NPError err = NPERR_NO_ERROR;

	switch (variable) {
		case NPPVpluginNeedsXEmbed:
			*((PRBool *)result) = PR_TRUE;
			break;

		case NPPVpluginScriptableNPObject:
			*((NPObject**) result) = getRootObject ();
			break;
		default:
			err = NPERR_INVALID_PARAM;
	}

	return err;
}

NPError
PluginInstance::SetValue (NPNVariable variable, void *value)
{
	return NPERR_NO_ERROR;
}

NPError 
PluginInstance::SetWindow (NPWindow* window)
{
	if (window == this->window)
		return NPERR_NO_ERROR;

	NPN_GetValue(this->instance, NPNVSupportsXEmbedBool, &this->xembed_supported);
	if (!this->xembed_supported)
	{
		DEBUGMSG ("*** XEmbed not supported");
		return NPERR_GENERIC_ERROR;
	}

	this->window = window;
	this->CreateWindow ();

	return NPERR_NO_ERROR;
}

void 
PluginInstance::CreateWindow ()
{
	DEBUGMSG ("*** creating window2 (%d,%d,%d,%d)", window->x, window->y, window->width, window->height);

	//  GtkPlug container and surface inside
	this->container = gtk_plug_new (reinterpret_cast <GdkNativeWindow> (window->window));

	// Connect signals to container
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (this->container), GTK_CAN_FOCUS);

	gtk_widget_add_events (
		this->container,
		GDK_BUTTON_PRESS_MASK | 
		GDK_BUTTON_RELEASE_MASK |
		GDK_KEY_PRESS_MASK | 
		GDK_KEY_RELEASE_MASK | 
		GDK_POINTER_MOTION_MASK |
		GDK_SCROLL_MASK |
		GDK_EXPOSURE_MASK |
		GDK_VISIBILITY_NOTIFY_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK |
		GDK_FOCUS_CHANGE_MASK
	);

	g_signal_connect (G_OBJECT(this->container), "event", G_CALLBACK (plugin_event_callback), this);

	this->surface = new Surface (window->width, window->height);

	if (background) {
		Color *c = color_from_str (background);
		surface->SetBackgroundColor (c);
		delete c;
	}

	gtk_container_add (GTK_CONTAINER (container), this->surface->GetDrawingArea());
	display = gdk_drawable_get_display (this->surface->GetDrawingArea()->window);
	gtk_widget_show_all (this->container);
	this->UpdateSource ();
}

void 
PluginInstance::UpdateSource ()
{
	if (!this->source)
		return;

	char *pos = strchr (this->source, '#');
	if (pos) {
		if (strlen(&pos[1]) > 0);
			this->UpdateSourceByReference (&pos[1]);
	} else {
		StreamNotify *notify = new StreamNotify (StreamNotify::SOURCE, this->source);
		NPN_GetURLNotify (this->instance, this->source, NULL, notify);
	}
}

void 
PluginInstance::UpdateSourceByReference (const char *value)
{
	NPObject *object = NULL;
	NPString reference;
	NPVariant result;

	if (NPERR_NO_ERROR != NPN_GetValue(this->instance, NPNVWindowNPObject, &object)) {
		DEBUGMSG ("*** Failed to get window object");
		return;
	}

	char jscript[strlen (value) + strlen (".text") + 1];

	g_strlcpy (jscript, value, sizeof(jscript));
	g_strlcat (jscript, ".text", sizeof(jscript));

	reference.utf8characters = jscript;
	reference.utf8length = strlen (jscript);

	if (NPN_Evaluate(this->instance, object, &reference, &result)) {
		if (NPVARIANT_IS_STRING (result)) {
			if (xaml_loader)
				delete xaml_loader;
			xaml_loader = PluginXamlLoader::FromStr (NPVARIANT_TO_STRING (result).utf8characters, this, this->surface);
			TryLoad ();
		}

		NPN_ReleaseVariantValue (&result);
	}

	NPN_ReleaseObject (object);
}

bool
PluginInstance::JsRunOnload ()
{
	bool retval = false;
	NPObject *object = NULL;
	NPVariant result;
	const char *expression = onLoad;

	if (NPERR_NO_ERROR != NPN_GetValue(instance, NPNVWindowNPObject, &object)) {
		DEBUGMSG ("*** Failed to get window object");
		return false;
	}

	if (!strncmp (expression, "javascript:", strlen ("javascript:")))
		expression += strlen ("javascript:");

	NPVariant args[1];

	DependencyObject *toplevel = surface->GetToplevel ();
	DEBUGMSG ("In JsRunOnload, toplevel = %p", toplevel);

	MoonlightEventObjectObject *depobj = EventObjectCreateWrapper (instance, toplevel);
	OBJECT_TO_NPVARIANT ((NPObject*)depobj, args[0]);

	if (NPN_Invoke (instance, object, NPID (expression),
			args, 1, &result)) {

		DEBUGMSG ("NPN_Invoke succeeded");
		NPN_ReleaseVariantValue (&result);

		retval = true;
	}
	else {
		DEBUGMSG ("NPN_Invoke failed");
	}
	NPN_ReleaseVariantValue (&args [0]);
	NPN_ReleaseObject (object);
	
	return retval;
}

NPError
PluginInstance::NewStream (NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
  //	DEBUGMSG ("NewStream (%s) %s", this->source, stream->url);

	if (IS_NOTIFY_SOURCE (stream->notifyData)) {
		*stype = NP_ASFILEONLY;
		return NPERR_NO_ERROR;
	} 

	if (IS_NOTIFY_DOWNLOADER (stream->notifyData)) {
		*stype = NP_ASFILE;
		return NPERR_NO_ERROR;
	} 

	if (IS_NOTIFY_REQUEST (stream->notifyData)) {
		*stype = NP_ASFILEONLY;
		return NPERR_NO_ERROR;
	}

	*stype = NP_NORMAL;

	return NPERR_NO_ERROR;
}

NPError
PluginInstance::DestroyStream (NPStream* stream, NPError reason)
{
	return NPERR_NO_ERROR;
}

//
// Tries to load the XAML file, the parsing might fail because a
// required dependency is not available, so we need to queue the
// request to fetch the data.
//
void
PluginInstance::TryLoad ()
{
	int error = 0;

	//
	// Only try to load if there's no missing files.
	//

	if (vm_missing_file == NULL)
		vm_missing_file = g_strdup (xaml_loader->TryLoad (&error));
	
	//printf ("PluginInstance::TryLoad, vm_missing_file: %s, error: %i\n", vm_missing_file, error);
	
	if (vm_missing_file != NULL){
		StreamNotify *notify = new StreamNotify (StreamNotify::REQUEST, vm_missing_file);
		NPN_GetURLNotify (instance, vm_missing_file, NULL, notify);
		return;
	}

	//
	// missing file was NULL, if error is set, display some message
	//
	if (!this->isLoaded && surface->GetToplevel ()) {
		this->isLoaded = true;
		if (this->onLoad)
			JsRunOnload ();
	}
}

char*
escape_quotes (char *s)
{
	char **parts;
	char *res;

	if (strchr (s, '\'') == NULL)
		return g_strdup (s);

	parts = g_strsplit (s, "'", 0);
	res = g_strjoinv ("\\'", parts);
	g_strfreev (parts);

	return res;
}

void
PluginInstance::ReportException (char *msg, char *details, char **stack_trace, int num_frames)
{
	NPObject *object = NULL;
	NPVariant result;
	char *script, *row_js, *msg_escaped, *details_escaped;
	char **stack_trace_escaped;
	NPString str;
	int i;
	bool res;

	// Get a reference to our element
	if (NPERR_NO_ERROR != NPN_GetValue(instance, NPNVPluginElementNPObject, &object)) {
		DEBUGMSG ("*** Failed to get plugin element object");
		return;
	}

	// FIXME:
	// - make sure the variables do not become global
	// - handle multiple calls

	// Remove ' from embedded strings
	msg_escaped = escape_quotes (msg);
	details_escaped = escape_quotes (details);
	stack_trace_escaped = g_new0 (char*, num_frames);
	for (i = 0; i < num_frames; ++i)
		stack_trace_escaped [i] = escape_quotes (stack_trace [i]);

	// JS code to create our elements
	row_js = g_strdup (" ");
	for (i = 0; i < num_frames; ++i) {
		char *s;

		s = g_strdup_printf ("%s%s%s", row_js, (i == 0) ? "" : "\\n ", stack_trace_escaped [i]);
		g_free (row_js);
		row_js = s;
	}

	script = g_strdup_printf ("text1 = document.createTextNode ('%s'); text2 = document.createTextNode ('Exception Details: '); text3 = document.createTextNode ('%s'); text4 = document.createTextNode ('Stack Trace:'); parent = this.parentNode; a = document.createElement ('div'); parent.insertBefore (a, this); a.appendChild (document.createElement ('hr')); msg = document.createElement ('font'); a.appendChild (msg); h2 = document.createElement ('h2'); i = document.createElement ('i'); b = document.createElement ('b'); msg.appendChild (h2); msg.appendChild (b); msg.appendChild (text3); msg.appendChild (document.createElement ('br')); msg.appendChild (document.createElement ('br')); b2 = document.createElement ('b'); b2.appendChild (text4); msg.appendChild (b2); b.appendChild (text2); h2.appendChild (i); i.appendChild (text1); msg.appendChild (document.createElement ('br')); msg.appendChild (document.createElement ('br')); a.appendChild (document.createElement ('hr')); table = document.createElement ('table'); msg.appendChild (table); table.width = '100%%'; table.bgColor = '#ffffcc'; tbody = document.createElement ('tbody'); table.appendChild (tbody); tr = document.createElement ('tr'); tbody.appendChild (tr); td = document.createElement ('td'); tr.appendChild (td); pre = document.createElement ('pre'); td.appendChild (pre); text = document.createTextNode ('%s'); pre.appendChild (text);", msg_escaped, details_escaped, row_js);

	g_free (msg_escaped);
	g_free (details_escaped);
	for (i = 0; i < num_frames; ++i)
		g_free (stack_trace_escaped [i]);
	g_free (stack_trace_escaped);
	g_free (row_js);

	str.utf8characters = script;
	str.utf8length = strlen (script);

	res = NPN_Evaluate (instance, object, &str, &result);
	if (res)
		NPN_ReleaseVariantValue (&result);
	NPN_ReleaseObject (object);
}

void
PluginInstance::StreamAsFile (NPStream* stream, const char* fname)
{
  //	DEBUGMSG ("StreamAsFile: %s", fname);

	if (IS_NOTIFY_SOURCE (stream->notifyData)) {
	  //		DEBUGMSG ("LoadFromXaml: %s", fname);
	  	if (xaml_loader)
	  		delete xaml_loader;
		xaml_loader = PluginXamlLoader::FromFilename (fname, this, this->surface);
		TryLoad ();
	}

	if (IS_NOTIFY_DOWNLOADER (stream->notifyData)){
		Downloader * dl = (Downloader *) ((StreamNotify *)stream->notifyData)->pdata;

		downloader_notify_finished (dl, fname);
	}
	
	if (IS_NOTIFY_REQUEST (stream->notifyData)) {
		bool reload = true;
		// printf ("PluginInstance::StreamAsFile: vm_missing_file: '%s', url: '%s', fname: '%s'.\n", vm_missing_file, stream->url, fname);

		if (!vm_missing_file)
			reload = false;
			
		if (reload && xaml_loader->GetMapping (vm_missing_file) != NULL) {
			// printf ("PluginInstance::StreamAsFile: the file '%s' has already been downloaded, won't try to reload xaml. Mapped to: '%s' (new url: '%s').", vm_missing_file, xaml_loader->GetMapping (vm_missing_file), stream->url);
			reload = false;
		}
		if (reload && xaml_loader->GetMapping (stream->url) != NULL) {
			// printf ("PluginInstance::StreamAsFile: the url '%s' has already been downloaded, won't try to reload xaml. Mapped to: '%s' (new url: '%s').", vm_missing_file, xaml_loader->GetMapping (stream->url), stream->url);
			reload = false;
		}
		
		if (vm_missing_file)
			xaml_loader->RemoveMissing (vm_missing_file);

		char* missing = vm_missing_file;
		vm_missing_file = NULL;

		if (reload) {
			// There may be more missing files.
			vm_missing_file = g_strdup (xaml_loader->GetMissing ());

			xaml_loader->InsertMapping (missing, fname);
			xaml_loader->InsertMapping (stream->url, fname);
			// printf ("PluginInstance::StreamAsFile: retry xaml loading, downloaded: %s to %s\n", missing, stream->url);
			
			// retry to load
			TryLoad ();
		}

		g_free (missing);
	}
}

int32
PluginInstance::WriteReady (NPStream* stream)
{
	//DEBUGMSG ("WriteReady (%d)", stream->end);

	StreamNotify *notify = STREAM_NOTIFY (stream->notifyData);

	if (notify && notify->pdata && IS_NOTIFY_DOWNLOADER (notify)) {
		Downloader * dl = (Downloader *) notify->pdata;
		downloader_notify_size (dl, stream->end);
		return MAX_STREAM_SIZE;
	}
	
	NPN_DestroyStream (instance, stream, NPRES_DONE);

	return -1L;
}

int32
PluginInstance::Write (NPStream* stream, int32 offset, int32 len, void* buffer)
{
	//DEBUGMSG ("Write size: %d offset: %d len: %d", stream->end, offset, len);

	StreamNotify *notify = STREAM_NOTIFY (stream->notifyData);

	if (notify && notify->pdata && IS_NOTIFY_DOWNLOADER (notify)) {
		Downloader * dl = (Downloader *) notify->pdata;
		downloader_write (dl, (guchar*) buffer, 0, len);
	}

	return len;
}

void
PluginInstance::UrlNotify (const char* url, NPReason reason, void* notifyData)
{
	StreamNotify* notify = STREAM_NOTIFY (notifyData);

	if (notify) 
		delete notify;
}

void
PluginInstance::Print (NPPrint* platformPrint)
{
	// nothing to do.
}

int16
PluginInstance::EventHandle (void* event)
{
	return 0;
}

/*** Getters and Setters ******************************************************/

void
PluginInstance::setSource (const char *value)
{
	if (!value || (this->source && !strcasecmp (this->source, value)))
		return;

	this->source = (char *) NPN_MemAlloc (strlen (value) + 1);
	strcpy (this->source, value);
	this->UpdateSource ();
}

char *
PluginInstance::getBackground ()
{
	return background;
}

void
PluginInstance::setBackground (const char *value)
{
	if (background)
		g_free (background);
	background = g_strdup (value);
	if (surface) {
		Color *c = color_from_str (background);
		surface->SetBackgroundColor (c);
		delete c;
	}
}

bool
PluginInstance::getEnableFramerateCounter ()
{
	return false;
}

bool
PluginInstance::getEnableRedrawRegions ()
{
	return false;
}

void
PluginInstance::setEnableRedrawRegions (bool value)
{
	// not implemented yet.
}

bool
PluginInstance::getEnableHtmlAccess ()
{
	return true;
}

bool
PluginInstance::getWindowless ()
{
	return this->windowless;
}

int32
PluginInstance::getActualHeight ()
{
	return surface->GetActualHeight ();
}

int32
PluginInstance::getActualWidth ()
{
	return surface->GetActualWidth ();
}

void
PluginInstance::getBrowserInformation (char **name, char **version,
				       char **platform, char **userAgent,
				       bool *cookieEnabled)
{
	*userAgent = (char*)NPN_UserAgent (instance);
	DEBUG_WARN_NOTIMPLEMENTED ("pluginInstance.getBrowserInformation");

	*name = "Foo!";
	*version = "Foo!";
	*platform = "Foo!";
	*cookieEnabled = true;
}

MoonlightScriptControlObject *
PluginInstance::getRootObject ()
{
	if (rootobject == NULL)
		rootobject = NPN_CreateObject (instance, MoonlightScriptControlClass);

	NPN_RetainObject (rootobject);
	return (MoonlightScriptControlObject*)rootobject;
}

NPP
PluginInstance::getInstance ()
{
	return instance;
}

int32
plugin_instance_get_actual_width (PluginInstance *instance)
{
	return instance->getActualWidth ();
}

int32
plugin_instance_get_actual_height (PluginInstance *instance)
{
	return instance->getActualHeight ();
}

char*
plugin_instance_get_init_params  (PluginInstance *instance)
{
	return instance->getInitParams();
}

uint32_t
plugin_html_timer_timeout_add (PluginInstance *instance, int32_t interval, GSourceFunc callback, gpointer data)
{
	uint32_t id;

#if GLIB_CHECK_VERSION(2,14,0)
	if (interval > 1000 && ((interval % 1000) == 0))
		id = g_timeout_add_seconds (interval / 1000, callback, data);
	else
#endif
		id = g_timeout_add (interval, callback, data);

	instance->timers = g_slist_append (instance->timers, GINT_TO_POINTER ((int)id));

	return id;
}

void
plugin_html_timer_timeout_stop (PluginInstance *instance, uint32_t source_id)
{
	g_source_remove (source_id);
	instance->timers = g_slist_remove (instance->timers, GINT_TO_POINTER (source_id));
}


void
plugin_instance_get_browser_information (PluginInstance *instance,
					 char **name, char **version,
					 char **platform, char **userAgent,
					 bool *cookieEnabled)
{
	instance->getBrowserInformation (name, version, platform, userAgent, cookieEnabled);
}

void
plugin_instance_report_exception (PluginInstance *instance, char *msg, char *details, char **stack_trace, int num_frames)
{
	instance->ReportException (msg, details, stack_trace, num_frames);
}

/*
	XamlLoader
*/


bool
PluginXamlLoader::LoadVM ()
{
#if INCLUDE_MONO_RUNTIME
	if (!vm_is_loaded ())
		vm_init ();
		
	if (vm_is_loaded ())
		return InitializeLoader ();
#endif

	return FALSE;
}

//
// On error it sets the @error ref to 1
// Returns the filename that we are missing
//
const char*
PluginXamlLoader::TryLoad (int *error)
{
	g_assert (GetFilename () == NULL ^ GetString () == NULL);
	
	*error = 0;

	DependencyObject* element;
	Type::Kind element_type;
	
	printf ("PluginXamlLoader::TryLoad, filename: %s, str: %s\n", GetFilename (), GetString ());
	
	if (GetFilename ()) {
		element = xaml_create_from_file (this, GetFilename (), true, &element_type);
	} else if (GetString ()) {
		element = xaml_create_from_str (this, GetString (), true, &element_type);
	} else {
		*error = 1;
		return NULL;
	}
	
	if (!element) {
		printf ("PluginXamlLoader::TryLoad: Could not load xaml %s: %s (missing_assembly: %s)\n", GetFilename () ? "file" : "string", GetFilename () ? GetFilename () : GetString (), GetMissing ());		
		return GetMissing ();
	}
	
	if (element_type != Type::CANVAS) {
		printf ("PluginXamlLoader::TryLoad: Return value is not a Canvas, its a %s\n", element->GetTypeName ());
		element->unref ();
		return NULL;
	}
	
	surface_attach (GetSurface (), (Canvas*) element);
	
	element->unref ();
	
	return NULL;
}

bool
PluginXamlLoader::HookupEvent (void* target, const char* name, const char* value)
{
	if (!XamlLoader::HookupEvent (target, name, value))
		event_object_add_javascript_listener ((EventObject*) target, plugin, name, value);
		
	return true;
}

bool
PluginXamlLoader::InitializeLoader ()
{
	if (initialized)
		return TRUE;
		
#if INCLUDE_MONO_RUNTIME
	if (!vm_is_loaded ())
		return FALSE;
		
	if (managed_loader)
		return TRUE;
		
	if (GetFilename ()) {
		managed_loader = vm_xaml_file_loader_new (this, plugin, GetSurface (), GetFilename ());
	} else if (GetString ()) {
		managed_loader = vm_xaml_str_loader_new (this, plugin, GetSurface (), GetString ());
	} else {
		return FALSE;
	}
	
	initialized = managed_loader != NULL;
#else
	initialized = TRUE;
#endif

	return initialized;
}

PluginXamlLoader::PluginXamlLoader (const char* filename, const char* str, PluginInstance* plugin, Surface* surface) : XamlLoader (filename, str, surface)
{
	this->plugin = plugin;
	this->initialized = FALSE;
#if INCLUDE_MONO_RUNTIME
	this->managed_loader = NULL;
#endif
}

PluginXamlLoader::~PluginXamlLoader ()
{
#if INCLUDE_MONO_RUNTIME
	if (managed_loader) {
		vm_loader_destroy (managed_loader);
	}
#endif
}

PluginXamlLoader* 
plugin_xaml_loader_from_str (const char* str, PluginInstance* plugin, Surface* surface)
{
	return PluginXamlLoader::FromStr (str, plugin, surface);
}


