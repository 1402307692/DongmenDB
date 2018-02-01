#include <unistd.h>
#include <string.h>
#include "shell.h"
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <dongmengsql.h>
#include <utils.h>


#define COL_SEPARATOR "|"

struct handler_entry handlers[] =
{
    HANDLER_ENTRY (open,      ".open FILENAME     Close existing database (if any) and open FILENAME"),
    HANDLER_ENTRY (parse,     ".parse \"SQL\"       Show parse tree for statement SQL"),
    HANDLER_ENTRY (opt,       ".optimizer \"SQL\"       Show parse tree and optimized parse tree for statement SQL"),
    HANDLER_ENTRY (headers,   ".headers on|off    Switch display of headers on or off in query results"),
    HANDLER_ENTRY (mode,      ".mode MODE         Switch display mode. MODE is one of:\n"
    		                  "                     column  Left-aligned columns\n"
    		                  "                     list    Values delimited by | (default)"),
    HANDLER_ENTRY (explain,   ".explain on|off    Turn output mode suitable for EXPLAIN on or off."),
    HANDLER_ENTRY (help,      ".help              Show this message"),
    HANDLER_ENTRY (exit,      ".exit              exit shell"),
    HANDLER_ENTRY (desc,      ".desc TABLENAME    desc table"),

    NULL_ENTRY
};


void usage_error(struct handler_entry *e, const char *msg)
{
    fprintf(stderr, "ERROR: %s\n", msg);
    fprintf(stderr, "%s\n", e->help);
}

int dongmengdb_shell_handle_cmd(dongmengdb_shell_handle_sql_t *ctx, const char *cmd)
{
    int rc = 0;

    if(cmd[0] == '.')
    {
        int h, ntokens;
        char *cmddup = strdup(cmd), **tokens;

        ntokens = dongmengdb_tokenize(cmddup, &tokens);

        for(h=0; handlers[h].name != NULL; h++)
        {
            if (!strncmp(tokens[0]+1, handlers[h].name, handlers[h].name_len))
            {
                rc = handlers[h].func(ctx, &handlers[h], (const char**) tokens, ntokens);
                break;
            }
        }

        if (handlers[h].name == NULL)
        {
            fprintf(stderr, "ERROR: Unrecognized command: %s\n", tokens[0]);
            free(tokens);
            free(cmddup);
            return 1;
        }
        else
        {
            free(tokens);
            free(cmddup);
            return rc;
        }
    }
    else
    {
        /* Anything that is not a command is a SQL statement,
         * which requires a DB file. */
        if(!ctx->db)
        {
            fprintf(stderr, "ERROR: No database is open.\n");
            return 1;
        }
        else
        {
            rc = dongmengdb_shell_handle_sql(ctx, cmd);
            return rc;
        }
    }

    return 0;
}

int dongmengdb_shell_handle_sql(dongmengdb_shell_handle_sql_t *ctx, const char *sql)
{
    int rc;
    dongmengdb_stmt *stmt;

    rc = dongmengdb_prepare(ctx->db, sql, &stmt);

    if (rc == DONGMENGDB_OK)
    {
        int numcol = dongmengdb_column_count(stmt);

        if(ctx->header)
        {
            for(int i = 0; i < numcol; i ++)
            {
                if(ctx->mode == MODE_LIST)
                {
                    printf(i==0?"":COL_SEPARATOR);
                    printf("%s", dongmengdb_column_name(stmt,i));
                }
                else if(ctx->mode == MODE_COLUMN)
                {
                    printf(i==0?"":" ");
                    printf("%-10.10s", dongmengdb_column_name(stmt,i));
                }
            }
            printf("\n");

            if(ctx->mode == MODE_COLUMN)
            {
                for(int i = 0; i < numcol; i ++)
                {
                   printf(i==0?"":" ");
                   printf("----------");
                }
                printf("\n");
            }
        }

        while((rc = dongmengdb_step(stmt)) == DONGMENGDB_ROW)
        {
            for(int i = 0; i < numcol; i++)
            {
                int coltype;

                if(ctx->mode == MODE_LIST)
                    printf(i==0?"":COL_SEPARATOR);
                else if (ctx->mode == MODE_COLUMN)
                    printf(i==0?"":" ");


                coltype = dongmengdb_column_type(stmt,i);

                if(coltype == SQL_NOTVALID)
                {
                    printf("ERROR: Column %i return an invalid type.\n", coltype);
                    break;
                }
                else if(coltype == SQL_INTEGER_1BYTE || coltype == SQL_INTEGER_2BYTE || coltype == SQL_INTEGER_4BYTE)
                {
                    if(ctx->mode == MODE_LIST)
                        printf("%i", dongmengdb_column_int(stmt,i));
                    else if (ctx->mode == MODE_COLUMN)
                        printf("%10i", dongmengdb_column_int(stmt,i));
                }
                else if(coltype == SQL_NULL)
                {
                    /* Print nothing */
                    if (ctx->mode == MODE_COLUMN)
                        printf("          ");
                }
                else
                {
                    int len;
                    if((coltype - 13) % 2 != 0)
                    {
                        printf("ERROR: Column %i returned an invalid type.\n", i);
                        break;
                    }
                    const char *text = dongmengdb_column_text(stmt,i);
                    len = strlen(text);
                    if(len != (coltype-13)/2)
                    {
                        printf("ERROR: THe length (%i) of the text in column %i does not match its type (%i).\n", len, i, coltype);
                        break;
                    }

                    if(ctx->mode == MODE_LIST)
                        printf("%s", text);
                    else if (ctx->mode == MODE_COLUMN)
                        printf("%-10.10s", text);
                }
            }
            printf("\n");
        }

        switch(rc)
        {
        case DONGMENGDB_ECONSTRAINT:
            printf("ERROR: SQL statement failed because of a constraint violation.\n");
            break;
        case DONGMENGDB_EMISMATCH:
            printf("ERROR: Data type mismatch.\n");
            break;
        case DONGMENGDB_EMISUSE:
            printf("ERROR: API used incorrectly.\n");
            break;
        case DONGMENGDB_ERROR_IO:
            printf("ERROR: An I/O error has occurred when accessing the file.\n");
            break;
        }

        rc = dongmengdb_finalize(stmt);
        if(rc == DONGMENGDB_EMISUSE)
            printf("API used incorrectly.\n");
    }
    else if (rc == DONGMENGDB_EINVALIDSQL)
        printf("SQL syntax error.\n");
    else if (rc == DONGMENGDB_ENOMEM)
        printf("ERROR: Could not allocate memory.\n");

    return rc;
}


int dongmengdb_shell_handle_cmd_open(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    int rc;
    dongmengdb *newdb = (dongmengdb *) malloc(sizeof(dongmengdb));

    if(ntokens != 2)
    {
    	usage_error(e, "Invalid arguments");
    	return 1;
    }
    char *token = strdup(tokens[1]);
    rc = dongmengdb_open(token, newdb);

	if (rc != DONGMENGDB_OK)
    {
        fprintf(stderr, "ERROR: Could not open file %s or file is not well formed.\n", tokens[1]);
        return rc;
    }


    if(ctx->db)
    {
    	dongmengdb_close(ctx->db);
    	free(ctx->dbfile);
    }

    ctx->db = newdb;
    ctx->dbfile = strdup(tokens[1]);

    return DONGMENGDB_OK;
}


int dongmengdb_shell_handle_cmd_parse(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    dongmengsql_statement_t *sqlStmt;
    int rc;

    if(ntokens != 2)
    {
    	usage_error(e, "Invalid arguments");
    	return 1;
    }

    rc = dongmengsql_parser(tokens[1], &sqlStmt);

    if (rc != DONGMENGDB_OK)
    {
        return rc;
    }

    dongmengsql_stmt_print(sqlStmt);
    printf("\n");

    return DONGMENGDB_OK;
}

/* Implemented in optimizer.c */
int dongmengdb_stmt_optimize(dongmengdb *db,
            dongmengsql_statement_t *sqlStmt,
            dongmengsql_statement_t **sqlStmtOpt);

int dongmengdb_shell_handle_cmd_opt(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    dongmengsql_statement_t *sqlStmt, *sqlStmtOpt;
    int rc;

    if(ntokens != 2)
    {
        usage_error(e, "Invalid arguments");
        return 1;
    }

    if(!ctx->db)
    {
        fprintf(stderr, "ERROR: No database is open.\n");
        return 1;
    }

    rc = dongmengsql_parser(tokens[1], &sqlStmt);

    if (rc != DONGMENGDB_OK)
    {
        return rc;
    }

    dongmengsql_stmt_print(sqlStmt);
    printf("\n\n");

    rc = dongmengdb_stmt_optimize(ctx->db, sqlStmt, &sqlStmtOpt);

    if(rc != DONGMENGDB_OK)
    {
        return rc;
    }

    dongmengsql_stmt_print(sqlStmtOpt);
    printf("\n");

    return DONGMENGDB_OK;
}

int dongmengdb_shell_handle_cmd_headers(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    if(ntokens != 2)
    {
    	usage_error(e, "Invalid arguments");
    	return 1;
    }

    if(strcmp(tokens[1],"on")==0)
        ctx->header = true;
    else if(strcmp(tokens[1],"off")==0)
        ctx->header = false;
    else
    {
    	usage_error(e, "Invalid argument");
    	return 1;
    }

    return DONGMENGDB_OK;
}

int dongmengdb_shell_handle_cmd_mode(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    if(ntokens != 2)
    {
    	usage_error(e, "Invalid arguments");
    	return 1;
    }

    if(strcmp(tokens[1],"list")==0)
        ctx->mode = MODE_LIST;
    else if(strcmp(tokens[1],"column")==0)
        ctx->mode = MODE_COLUMN;
    else
    {
    	usage_error(e, "Invalid argument");
    	return 1;
    }

    return DONGMENGDB_OK;
}

int dongmengdb_shell_handle_cmd_explain(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    if(ntokens != 2)
    {
    	usage_error(e, "Invalid arguments");
    	return 1;
    }

    if(strcmp(tokens[1],"on")==0)
    {
        ctx->header = true;
        ctx->mode = MODE_COLUMN;
    }
    else if(strcmp(tokens[1],"off")==0)
    {
        ctx->header = false;
        ctx->mode = MODE_LIST;
    }
    else
    {
    	usage_error(e, "Invalid argument");
    	return 1;
    }

    return DONGMENGDB_OK;
}

int dongmengdb_shell_handle_cmd_help(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens)
{
    for(int h=0; handlers[h].name != NULL; h++)
    {
    	fprintf(stderr, "%s\n", handlers[h].help);
    }

    return DONGMENGDB_OK;
}

int dongmengdb_shell_handle_cmd_exit(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens){
    if(ctx->db)
    {
        dongmengdb_close(ctx->db);
        free(ctx->dbfile);
    }

    exit(0);
};

int dongmengdb_shell_handle_cmd_desc(dongmengdb_shell_handle_sql_t *ctx, struct handler_entry *e, const char **tokens, int ntokens){
    if (ctx->db){
        char *token = (char *)tokens[1];
        table_info *tableInfo = table_manager_get_tableinfo(ctx->db->metadataManager->tableManager,token, ctx->db->tx);
    }
    return DONGMENGDB_OK;
};
