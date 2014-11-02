/*
 * commands.c               -- custom commands mcabber plugin
 *
 * Copyright (C) 2012 Dmitry Potapov <potapov.d@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAX_ARGS
#define MAX_ARGS 32
#endif

#include <stdlib.h>
#include <string.h>

#include <mcabber/modules.h>
#include <mcabber/commands.h>
#include <mcabber/compl.h>
#include <mcabber/hooks.h>
#include <mcabber/logprint.h>

struct custom_command{
    char* name;
    GRegex* buddy_regex;
    GRegex* input_regex;
    char* command;
    guint completion_id;
};

static GSList* custom_commands;
static guint msg_in_hid;
static guint custom_commands_compl_categ;

static gchar* print_command(char* prev, char* curr, char* args) {
    gchar* tmp = g_strndup(prev, curr - prev);
    gchar* result = g_strdup_printf(tmp, args);
    g_free(tmp);
    return result;
}

static void handle_command(char* args, gpointer data)
{
    GError* err = 0;
    struct custom_command* command = (struct custom_command*)data;
    gchar* argv[MAX_ARGS + 2] = {0};
    ssize_t argc = 0;
    char* prev = command->command;
    char* ptr = command->command;
    while (argc < MAX_ARGS && *ptr) {
        if (*ptr == '"') {
            prev = ++ptr;
            while (*ptr && *ptr != '"') {
                ++ptr;
            }
            argv[argc++] = print_command(prev, ptr, args);
            if (*ptr == '"') {
                if (*++ptr) {
                    ++ptr;
                }
            }
            prev = ptr;
        } else {
            while (*ptr && *ptr != ' ') {
                ++ptr;
            }
            argv[argc++] = print_command(prev, ptr, args);
            if (*ptr) {
                ++ptr;
            }
            prev = ptr;
        }
    }
    g_spawn_async(
        0,
        argv,
        0,
        G_SPAWN_SEARCH_PATH
        | G_SPAWN_STDOUT_TO_DEV_NULL
        | G_SPAWN_STDERR_TO_DEV_NULL,
        0,
        0,
        0,
        &err);
    if (err) {
        scr_log_print(
            LPRINT_NORMAL,
            "Failed to execute command '%s' with arg '%s': %s",
            command->command,
            args,
            err->message);
        g_error_free(err);
    }
    while (--argc >= 0) {
        g_free(argv[argc]);
    }
}

static void delete_custom_command(gpointer data)
{
    struct custom_command* command = (struct custom_command*)data;
    cmd_del(command->name);
    g_free(command->name);
    g_regex_unref(command->buddy_regex);
    g_regex_unref(command->input_regex);
    g_free(command->command);
    compl_del_category(command->completion_id);
    g_free(data);
}

static char* next_token(char* args)
{
    char* end;
    char* p = args;
    if (args) {
        while (*p == ' ') {
            ++p;
        }
        end = p;
        while (*end && *end != ' ') {
            ++end;
        }
        if (*end) {
            *end = 0;
            return p;
        }
    }
    return 0;
}

static void del_custom_command(char* args)
{
    GSList* cmd;
    struct custom_command* command;
    for (cmd = custom_commands; cmd; cmd = g_slist_next(cmd)) {
        command = (struct custom_command*)cmd->data;
        if (!strcmp(command->name, args)) {
            compl_del_category_word(custom_commands_compl_categ,
                command->name);
            delete_custom_command(command);
            custom_commands = g_slist_delete_link(custom_commands, cmd);
            return;
        }
    }
}

static void add_custom_command(char* args)
{
    GError* err = 0;
    char* name = next_token(args);
    char* buddy_regex = next_token(name ? (name + strlen(name) + 1) : 0);
    char* input_regex =
        next_token(buddy_regex ? (buddy_regex + strlen(buddy_regex) + 1) : 0);
    if (!input_regex) {
        scr_log_print(LPRINT_NORMAL, "The syntax are: /add_custom_command "
            "<command name> <buddy regex> <completion regexp> <command>. "
            "You've specified: '%s' '%s' '%s' '%s'", name, buddy_regex,
            input_regex, "(null)");
        return;
    }
    del_custom_command(name);
    struct custom_command* command = g_malloc0(sizeof(struct custom_command));
    command->name = g_strdup(name);
    command->buddy_regex = g_regex_new(buddy_regex,
        G_REGEX_OPTIMIZE|G_REGEX_ANCHORED, G_REGEX_MATCH_ANCHORED, &err);
    if (err) {
        scr_log_print(
            LPRINT_NORMAL,
            "Failed to compile buddy name pattern '%s': %s",
            buddy_regex,
            err->message);
        goto error;
    }
    command->input_regex = g_regex_new(input_regex, G_REGEX_OPTIMIZE, 0, &err);
    if (err) {
        scr_log_print(
            LPRINT_NORMAL,
            "Failed to compile input pattern '%s': %s",
            input_regex,
            err->message);
        goto error;
    }
    command->command = g_strdup(input_regex + strlen(input_regex) + 1);
    command->completion_id = compl_new_category(0x30);
    // TODO: pass some meaningful help string
    cmd_add(command->name, "", command->completion_id, 0,
        (void (*)(char*))handle_command, command);
    custom_commands = g_slist_prepend(custom_commands, command);
    compl_add_category_word(custom_commands_compl_categ, name);
    return;
error:
    g_error_free(err);
    g_free(command->name);
    if (command->buddy_regex) {
        g_regex_unref(command->buddy_regex);
    }
    g_free(command);
}

static void list_custom_commands(char* args)
{
    GSList* cmd;
    struct custom_command* command;
    gchar *newstr;
    gchar *str = "";
    (void) args;

    for (cmd = custom_commands; cmd; cmd = g_slist_next(cmd)) {
        command = (struct custom_command*)cmd->data;
        newstr = g_strdup_printf("%s %s,", str, command->name);
        if (*str) {
            g_free(str);
        }
        str = newstr;
    }
    if (custom_commands) {
        *(str + strlen(str) - 1) = '.';
        scr_log_print(LPRINT_NORMAL, "Available custom commands are:%s", str);
        g_free(str);
    } else {
        scr_log_print(LPRINT_NORMAL, "There is no custom commands found");
    }
}

static void show_custom_command(char* args)
{
    guint dynlist;

    for (GSList* cmd = custom_commands; cmd; cmd = g_slist_next(cmd)) {
        struct custom_command* command = (struct custom_command*)cmd->data;
        if (!strcmp(command->name, args)) {
            scr_log_print(LPRINT_NORMAL, "Custom command '%s' greps messages "
                "from user(s) '%s' for strings matching '%s' and executes "
                "command '%s'. Currently, this command has %d completion(s) "
                "available",
                command->name, g_regex_get_pattern(command->buddy_regex),
                g_regex_get_pattern(command->input_regex), command->command,
                g_slist_length(compl_get_category_list(command->completion_id,
                    &dynlist)));
            return;
        }
    }
    scr_log_print(LPRINT_NORMAL, "No such custom command");
}

static guint parse_message(const gchar *hookname, hk_arg_t *args,
    gpointer userdata)
{
    GSList* cmd;
    const gchar* bjid = 0;
    const gchar* message = 0;
    gchar* word;
    GMatchInfo* match_info;
    struct custom_command* command;
    (void) hookname;
    (void) userdata;

    for (; args->name; ++args) {
        if (!bjid && !g_strcmp0(args->name, "jid")) {
            bjid = args->value;
        } else if (!message && !g_strcmp0(args->name, "message")) {
            message = args->value;
        }
    }

    if (bjid && message) {
        for (cmd = custom_commands; cmd; cmd = g_slist_next(cmd)) {
            command = (struct custom_command*)cmd->data;
            if (g_regex_match(command->buddy_regex, bjid, 0, NULL)) {
                g_regex_match(command->input_regex, message, 0, &match_info);
                while (g_match_info_matches(match_info)) {
                    word = g_match_info_fetch(match_info, 0);
                    compl_del_category_word(command->completion_id, word);
                    compl_add_category_word(command->completion_id,
                        word);
                    g_free(word);
                    g_match_info_next(match_info, NULL);
                }
                g_match_info_free(match_info);
            }
        }
    }
    return HOOK_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void commands_init()
{
    custom_commands = NULL;
    msg_in_hid = hk_add_handler(parse_message, HOOK_PRE_MESSAGE_IN,
        G_PRIORITY_DEFAULT_IDLE, NULL);
    custom_commands_compl_categ = compl_new_category(0x30);
    cmd_add("add_custom_command", "Add custom command", 0, 0,
        add_custom_command, NULL);
    cmd_add("del_custom_command", "Delete custom command",
        custom_commands_compl_categ, 0, del_custom_command, NULL);
    cmd_add("list_custom_commands", "Print custom commands list", 0, 0,
        list_custom_commands, NULL);
    cmd_add("show_custom_command", "Print custom command information",
        custom_commands_compl_categ, 0, show_custom_command, NULL);
    cmd_set_safe("add_custom_command", TRUE);
}

static void commands_uninit()
{
    g_slist_free_full(custom_commands, delete_custom_command);
    hk_del_handler(HOOK_PRE_MESSAGE_IN, msg_in_hid);
    cmd_del("add_custom_command");
    cmd_del("del_custom_command");
    cmd_del("list_custom_commands");
    cmd_del("show_custom_command");
    compl_del_category(custom_commands_compl_categ);
}

/* Module description */
module_info_t info_commands = {
        .branch         = MCABBER_BRANCH,
        .api            = MCABBER_API_VERSION,
        .version        = "0.05",
        .description    = "custom commands plugin",
        .requires       = NULL,
        .init           = commands_init,
        .uninit         = commands_uninit,
        .next           = NULL,
};

