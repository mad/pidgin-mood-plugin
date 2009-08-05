/*
 * Pidgin simple mood support
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Known bugs:
 *
 * - Icon mood button left side;
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <internal.h>
#endif

#include <plugin.h>
#include <imgstore.h>
#include <account.h>
#include <core.h>
#include <debug.h>
#include <gtkconv.h>
#include <util.h>
#include <version.h>
#include <gtkplugin.h>
#include <gtkimhtml.h>
#include <gtkutils.h>
#include <gtknotify.h>
#include <conversation.h>

#define DBGID "mood"
#define PREF_PREFIX "/plugins/gtk/mood-plugin"
#define PREF_MOOD_PATH PREF_PREFIX "/moods_path"

struct mood_button_list {
  GtkWidget *button;
  gchar *text;
  struct mood_button_list *next;
};

static const char * const moodstrings[] = {
        "afraid",
        "amazed",
        "angry",
        "annoyed",
        "anxious",
        "aroused",
        "ashamed",
        "bored",
        "brave",
        "calm",
        "cold",
        "confused",
        "contented",
        "cranky",
        "curious",
        "depressed",
        "disappointed",
        "disgusted",
        "distracted",
        "embarrassed",
        "excited",
        "flirtatious",
        "frustrated",
        "grumpy",
        "guilty",
        "happy",
        "hot",
        "humbled",
        "humiliated",
        "hungry",
        "hurt",
        "impressed",
        "in_awe",
        "in_love",
        "indignant",
        "interested",
        "intoxicated",
        "invincible",
        "jealous",
        "lonely",
        "mean",
        "moody",
        "nervous",
        "neutral",
        "offended",
        "playful",
        "proud",
        "relieved",
        "remorseful",
        "restless",
        "sad",
        "sarcastic",
        "serious",
        "shocked",
        "shy",
        "sick",
        "sleepy",
        "stressed",
        "surprised",
        "thirsty",
        "worried",
        NULL
};

char *current_mood;

static void mood_make_stanza(PurpleConnection *, char **, gpointer );
static void mood_create_button(PidginConversation *);
static void mood_make_button_list(GtkWidget *, struct mood_button_list *);
static void mood_make_dialog_cb(GtkWidget *);
static void mood_button_cb(GtkWidget *, GtkWidget *);
static GtkWidget *mood_button_from_file(const char *, char *, GtkWidget *);
static void mood_set_path_cb (GtkWidget *, GtkWidget *);

static void disconnect_prefs_callbacks(GtkObject *, gpointer );


static void
mood_make_stanza(PurpleConnection *gc, char **packet, gpointer null)
{
  xmlnode *moodnode;

  if (!current_mood || !xmlnode_get_child((xmlnode*)*packet, "body"))
    return;

  purple_debug_misc(DBGID, "make mood stanza\n");

  moodnode = xmlnode_new("mood");
  xmlnode_set_namespace(moodnode, "http://jabber.org/protocol/mood");
  xmlnode_new_child(moodnode, current_mood);

  xmlnode_insert_child((xmlnode*)*packet, moodnode);

  g_free(current_mood);
  current_mood = NULL;
}

static void
mood_create_button(PidginConversation *gtkconv)
{
  GtkWidget *mood_button, *image, *bbox;
  PurpleConversation *conv;
  const char *who;
  gchar *tmp, *mood_path;
  gchar *protocol;

  conv = gtkconv->active_conv;
  who = purple_conversation_get_name(conv);
  protocol = purple_account_get_protocol_name(conv->account);

  purple_debug_misc(DBGID, "%s\n", protocol);

  // mood only XMPP proto
  if(strcmp("XMPP", protocol) != 0)
    return;

  mood_button = g_object_get_data(G_OBJECT(gtkconv->toolbar), "mood_button");

  if (mood_button)
    return;

  mood_button = gtk_button_new();
  bbox = gtk_vbox_new(FALSE, 0);
  g_object_set_data(G_OBJECT(gtkconv->toolbar), "mood_bbox", bbox);
  g_object_set_data(G_OBJECT(gtkconv->toolbar), "mood_button", mood_button);

  gtk_button_set_relief(GTK_BUTTON(mood_button), GTK_RELIEF_NONE);
  gtk_container_add(GTK_CONTAINER(mood_button), bbox);
  mood_path = purple_prefs_get_string(PREF_MOOD_PATH);
  tmp = g_strdup_printf("%s/amazed.png", mood_path);
  image = gtk_image_new_from_file(tmp);
  g_free(tmp);
  gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(mood_button), "clicked", G_CALLBACK(mood_make_dialog_cb),
		   NULL);

  gtk_box_pack_start(GTK_BOX(gtkconv->toolbar), mood_button, FALSE, FALSE, 0);

  gtk_widget_show_all(bbox);
  gtk_widget_show(mood_button);
}

static void
mood_make_dialog_cb(GtkWidget *fake)
{
  GtkWidget *dialog, *vbox;
  GtkWidget *mood_table = NULL;
  GtkWidget *scrolled, *viewport;

  struct mood_button_list *ml, *prev_ml = NULL, *tmp_ml = NULL;
  gchar *mood_full_path, *mood_path;
  int i;

  dialog = pidgin_create_dialog("Mood", 0, "mood_dialog", FALSE);
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
  vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(dialog), FALSE, 0);

  mood_table = gtk_vbox_new(FALSE, 0);

  for (i = 0; moodstrings[i]; ++i) {
    ml = g_new0(struct mood_button_list, 1);
    if (!tmp_ml) {
      tmp_ml = ml;
    }
    mood_path = purple_prefs_get_string(PREF_MOOD_PATH);
    mood_full_path = g_strdup_printf("%s/%s.png", mood_path, moodstrings[i]);
    ml->text = g_strdup(moodstrings[i]);
    ml->button = mood_button_from_file(mood_full_path, moodstrings[i], dialog);
    ml->next = NULL;

    if (prev_ml) {
      prev_ml->next = ml;
    }
    prev_ml = ml;
    ml = ml->next;

    g_free(mood_full_path);
  }
  ml = tmp_ml;

  mood_make_button_list(mood_table, ml);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled),
                                      GTK_SHADOW_NONE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled),
                                 GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
  gtk_widget_show(scrolled);

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                        mood_table);
  gtk_widget_show(mood_table);

  viewport = gtk_widget_get_parent(mood_table);
  gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);

  g_signal_connect(G_OBJECT(dialog), "destroy",
                   G_CALLBACK(gtk_widget_destroy), NULL);

  while (ml) {
    struct mood_button_list *tmp = ml->next;
    g_free(ml->text);
    g_free(ml);
    ml = tmp;
  }

  gtk_widget_show_all(dialog);
}

GtkWidget *
mood_button_from_file(const char *filename, char *mood, GtkWidget *dialog)
{
  GtkWidget *button, *image;

  button = gtk_button_new();

  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_object_set_data(G_OBJECT(button), "mood_text", mood);

  image = gtk_image_new_from_file(filename);

  gtk_container_add(GTK_CONTAINER(button), image);

  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(mood_button_cb), dialog);

  return button;
}


static void
mood_button_cb(GtkWidget *widget, GtkWidget *dialog)
{
  char *mood_text;

  mood_text = g_object_get_data(G_OBJECT(widget), "mood_text");

  current_mood = g_strdup(mood_text);

  purple_debug_misc(DBGID, "make message hook %s\n", mood_text);

  gtk_widget_destroy(dialog);
  return;
}

static void
mood_make_button_list(GtkWidget *container, struct mood_button_list *list)
{
  GtkWidget *line;
  int mood_count = 0;

  if (!list)
    return;

  line = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);

  for (; list; list = list->next) {
    gtk_box_pack_start(GTK_BOX(line), list->button, FALSE, FALSE, 0);
    gtk_widget_show(list->button);
    mood_count++;
    if (mood_count > 5) {
      if (list->next) {
        line = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);
      }
      mood_count = 0;
    }
  }
}

static void
mood_remove_button(PidginConversation *gtkconv)
{
  GtkWidget *mood_button, *mood_bbox;

  mood_button = g_object_get_data(G_OBJECT(gtkconv->toolbar),
                                  "mood_button");
  mood_bbox = g_object_get_data(G_OBJECT(gtkconv->toolbar),
                                "mood_bbox");

  if (mood_button) {
    gtk_widget_destroy(mood_button);
    gtk_widget_destroy(mood_bbox);
    g_object_set_data(G_OBJECT(gtkconv->toolbar), "mood_bbox", NULL);
    g_object_set_data(G_OBJECT(gtkconv->toolbar), "mood_button", NULL);
  }
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
  GList *convs = purple_get_conversations();
  PidginConversation *gtk_conv_handle = pidgin_conversations_get_handle();
  PurplePlugin *jabber;

  /* for mood stanza (only jabber conv) */
  jabber = purple_find_prpl("prpl-jabber");

  if (jabber) {
    purple_signal_connect(jabber, "jabber-sending-xmlnode", plugin,
			  PURPLE_CALLBACK(mood_make_stanza), NULL);
    purple_signal_connect(gtk_conv_handle, "conversation-displayed", plugin,
			  PURPLE_CALLBACK(mood_create_button), NULL);
  }

  // XXX: purple_conversation_foreach (init_conversation); ?
  while (convs) {
    PurpleConversation *conv = (PurpleConversation *)convs->data;
    PidginConversation *gtkconv;

    gtkconv = PIDGIN_CONVERSATION(conv);

    if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
      mood_create_button(PIDGIN_CONVERSATION(conv));
    }
    convs = convs->next;
  }

  return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
  GList *convs = purple_get_conversations();

  while (convs) {
    PurpleConversation *conv = (PurpleConversation *)convs->data;
    if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
      mood_remove_button(PIDGIN_CONVERSATION(conv));
    }
    convs = convs->next;
  }
  return TRUE;
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
  GtkWidget *ret;
  GtkWidget *frame;
  GtkWidget *vbox, *hbox;
  GtkWidget *mood_path, *mood_path_label, *mood_path_button;

  ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
  gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

  g_signal_connect(GTK_OBJECT(ret), "destroy", G_CALLBACK(disconnect_prefs_callbacks), plugin);


  frame = pidgin_make_frame(ret, "Settings");
  vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
  gtk_box_pack_start(GTK_BOX(frame), vbox, FALSE, FALSE, 0);

  /* Path to moods dir */
  hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  mood_path = gtk_entry_new();
  mood_path_label = gtk_label_new("Mood image path");
  mood_path_button = gtk_button_new_with_mnemonic("Apply");

  gtk_entry_set_text((GtkEntry*)mood_path, purple_prefs_get_string(PREF_MOOD_PATH));

  g_signal_connect(G_OBJECT(mood_path_button), "clicked",
		   G_CALLBACK(mood_set_path_cb), mood_path);

  gtk_box_pack_start(GTK_BOX(hbox), mood_path_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), mood_path, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), mood_path_button, FALSE, FALSE, 0);

  gtk_widget_show_all(ret);
  return ret;
}

static void
mood_set_path_cb (GtkWidget *button, GtkWidget *text_field)
{
  const char * path = gtk_entry_get_text((GtkEntry*)text_field);
  purple_prefs_set_string(PREF_MOOD_PATH, path);
}

static void
disconnect_prefs_callbacks(GtkObject *object, gpointer data)
{
  PurplePlugin *plugin = (PurplePlugin *)data;

  purple_prefs_disconnect_by_handle(plugin);
}

static PidginPluginUiInfo ui_info =
  {
    get_config_frame,
    0, /* page_num (reserved) */

    /* padding */
    NULL,
    NULL,
    NULL,
    NULL
  };


static PurplePluginInfo info =
  {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,                       /**< major version */
    PURPLE_MINOR_VERSION,                       /**< minor version */
    PURPLE_PLUGIN_STANDARD,                     /**< type */
    PIDGIN_PLUGIN_TYPE,                         /**< ui_requirement */
    0,                                          /**< flags */
    NULL,                                       /**< dependencies */
    PURPLE_PRIORITY_DEFAULT,                    /**< priority */

    "gtkmood",                                  /**< id */
    "mood",                                     /**< name */
    "0.1",                                      /**< version */
    "Simple mood support.",                     /**< summary */
    "Simple mood support.",                     /**< description */
    "owner.mad.epa@gmail.com",                  /**< author */
    "http://github.com/mad/pidgin-mood-plugin", /**< homepage */
    plugin_load,                                /**< load */
    plugin_unload,                              /**< unload */
    NULL,                                       /**< destroy */
    &ui_info,                                   /**< ui_info        */
    NULL,                                       /**< extra_info */
    NULL,                                       /**< prefs_info */
    NULL,                                       /**< actions */
                                                /* padding */
    NULL,
    NULL,
    NULL,
    NULL
  };


static void
init_plugin(PurplePlugin *plugin)
{
  char *home;
  char *mood_path;

#ifdef _WIN32
  home = getenv("APPDATA");
#else
  home = getenv("HOME");
#endif

  mood_path = g_strdup_printf("%s/.purple/plugins/moods/", home);
  purple_prefs_add_string(PREF_MOOD_PATH, mood_path);

  g_free(mood_path);
}

PURPLE_INIT_PLUGIN(mood, init_plugin, info)
