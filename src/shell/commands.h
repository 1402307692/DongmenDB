/*
 * commands.h
 *
 *  Created on: Jul 31, 2014
 *      Author: borja
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

int dongmengdb_shell_handle_cmd(dongmengdb_shell_handle_sql_t *ctx, const char *cmd);
int dongmengdb_shell_handle_sql(dongmengdb_shell_handle_sql_t *ctx, const char *sql);

struct handler_entry;

typedef int (*handler_function)(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);

struct handler_entry
{
    char *name;
    char *help;
    int name_len;
    handler_function func;
};

#define HANDLER_ENTRY(NAME,HELP) { #NAME, HELP, sizeof #NAME - 1, dongmengdb_shell_handle_cmd_ ## NAME}
#define NULL_ENTRY               { NULL, NULL, 0, NULL }

int dongmengdb_shell_handle_cmd_open(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_help(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_parse(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_opt(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_mode(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_headers(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_explain(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_exit(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);
int dongmengdb_shell_handle_cmd_desc(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens);

#endif /* COMMANDS_H_ */
