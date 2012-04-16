#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <glib/gstdio.h>

#include <mcabber/modules.h>
#include <mcabber/commands.h>
#include <mcabber/settings.h>
#include <mcabber/compl.h>
#include <mcabber/hooks.h>
#include <mcabber/utils.h>
#include <mcabber/logprint.h>

struct custom_command{
    char* name;
    char* buddy_regex_string;
    GRegex* buddy_regex;
    char* input_regex_string;
    GRegex* input_regex;
    char* command;
    guint completion_id;
};

GSList* custom_commands;
guint msg_in_hid;
guint custom_commands_compl_categ;

static void delete_compl_categ(guint categ)
{
    guint dynlist;
    GSList* list = compl_get_category_list(categ, &dynlist);
    g_slist_free_full(list, g_free);
    compl_del_category(categ);
}

static void delete_custom_command(gpointer data)
{
    struct custom_command* command = (struct custom_command*)data;
    cmd_del(command->name);
    g_free(command->name);
    g_free(command->buddy_regex_string);
    g_regex_unref(command->buddy_regex);
    g_free(command->input_regex_string);
    g_regex_unref(command->input_regex);
    g_free(command->command);
    delete_compl_categ(command->completion_id);
    g_free(data);
}

static void add_custom_command(char* args)
{
}

static void del_custom_command(char* args)
{
}

static void list_custom_commands(char* args)
{
}

static void show_custom_command(char* args)
{
}

static guint parse_message(const gchar *hookname, hk_arg_t *args,
                                     gpointer userdata)
{
    return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void commands_init()
{
    custom_commands = NULL;
    msg_in_hid = hk_add_handler(parse_message, HOOK_PRE_MESSAGE_IN,
        G_PRIORITY_DEFAULT_IDLE, NULL);
    custom_commands_compl_categ = compl_new_category();
    compl_add_unordered_category_word(custom_commands_compl_categ, "hello");
    compl_add_unordered_category_word(custom_commands_compl_categ, "workl");
    cmd_add("add_custom_command", "Add custom command", 0, 0,
        add_custom_command, NULL);
    cmd_add("del_custom_command", "Delete custom command",
        custom_commands_compl_categ, 0, del_custom_command, NULL);
    cmd_add("list_custom_commands", "Print custom commands list", 0, 0,
        list_custom_commands, NULL);
    cmd_add("show_custom_command", "Print custom command information",
        custom_commands_compl_categ, 0, show_custom_command, NULL);
}

static void commands_uninit()
{
    g_slist_free_full(custom_commands, delete_custom_command);
    hk_del_handler(HOOK_PRE_MESSAGE_IN, msg_in_hid);
    cmd_del("add_custom_command");
    cmd_del("del_custom_command");
    cmd_del("list_custom_commands");
    cmd_del("show_custom_command");
    delete_compl_categ(custom_commands_compl_categ);
}

/* Module description */
module_info_t info_commands = {
        .branch         = MCABBER_BRANCH,
        .api            = MCABBER_API_VERSION,
        .version        = "0.01",
        .description    = "custom commands plugin",
        .requires       = NULL,
        .init           = commands_init,
        .uninit         = commands_uninit,
        .next           = NULL,
};

