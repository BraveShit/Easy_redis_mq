/*##############################################################################
** 文 件 名: readconf.c
** Copyright (c), 1991-2013, Telpo Communication CO.,Ltd.
** 创 建 人: lihuaxuan
** 日    期: 2013-10-31
** 描    述:
** 版    本:
** 修改历史:
** 2013-10-31,   创建本文件；
##############################################################################*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "readconf.h"
//#include "log.h"

/*############################## Global Variable #############################*/
#define MAX_LINE_SIZE 2048


/*############################## function #############################*/
static int cfg_parse_line(int *line_n, char *line, struct _CFG **conf, struct _CFG **clast, struct _KEY **klast)
{
    struct _CFG *cnode;
    struct _KEY *knode;
    char *temp, *temp2, *temp3;

    (*line_n)++;
    temp = line + 1;

    if (*temp == ';')    // ';'  是注释符号
        return 0;

    temp = (char *)strchr(temp, ';');    
    if (temp)
        *temp = 0;

    temp = line + 1;
    while ((*temp == ' ') || (*temp == '\t'))
        temp++;

    temp2 = temp + strlen(temp) - 1;
    while ((*temp2 == ' ') || (*temp2 == '\t') || (*temp2 == '\n') || (*temp2 == '\r'))
        *temp2-- = 0;

    if (!*temp)        /* ignore blanks */
        return 0;

    if (*temp == '[')
    {
        temp++;

        temp2 = (char *)strchr(temp, ']');
        if (!temp2)
        {
            //rs_log(LOG_ERROR, "Config line #%i missing ], %s\n", *line_n, line);
            return -1;
        }

        *temp2-- = 0;
#if 0        
        if (*(temp2 + 2))
        {
            rs_log(LOG_ERROR, "Junk after ] in line #%i, %s", *line_n, line);
            return -1;
        }
#endif

        while ((*temp2 == ' ') || (*temp2 == '\t'))
            *temp2-- = 0;

        if (!*temp)
        {
            //rs_log(LOG_ERROR, "Missing APP on line #%i, %s\n", *line_n, line);
            return -1;
        }

        for (cnode = *conf; cnode; cnode = cnode->next)
            if (strcasecmp(cnode->app, temp) == 0)
            {
                //rs_log(LOG_ERROR, "Duplicate APP values in line #%i, %s\n", *line_n, line);
                return -1;
            }

        cnode = (struct _CFG *)malloc(sizeof(struct _CFG));
        memset(cnode, 0, sizeof(struct _CFG));

        cnode->app = (char *)malloc(strlen(temp) + 1);
        strcpy(cnode->app, temp);

//        rs_log(LOG_DEBUG, "App[%s]\n", temp);
        
        if (!*clast)
            *conf = *clast = cnode;
        else
        {
            (*clast)->next = cnode;
            *clast = cnode;
        }

        *klast = knode = NULL;
    }
    else if ((temp2 = (char *)strchr(temp, '=')) != NULL)
    {
        if (!(*clast))
        {
            //rs_log(LOG_ERROR, "No APP value specified by line #%i, %s\n", *line_n, line);
            return -1;
        }

        temp3 = temp2 - 1;
        while ((*temp3 == ' ') || (*temp3 == '\t'))
            *temp3-- = 0;

        *temp2++ = 0;
        while ((*temp2 == '>')||(*temp2 == ' ') || (*temp2 == '\t'))
            temp2++;

        if (!*temp)
        {
            //rs_log(LOG_ERROR, "Missing KEY on line #%i, %s\n", *line_n, line);
            return -1;
        }

        for (knode = (*clast)->key; knode; knode = knode->next)
            if (strcasecmp(knode->name, temp) == 0)
            {
                //rs_log(LOG_ERROR, "Duplicate KEY on line #%i, %s\n", *line_n, line);
                return -1;
            }

        knode = (struct _KEY *)malloc(sizeof(struct _KEY));
        memset(knode, 0, sizeof(struct _KEY));

        knode->name = (char *)malloc(strlen(temp) + 1);
        strcpy(knode->name, temp);
//        rs_log(LOG_DEBUG, "Key[%s]\n", temp);
        if (!*temp2)
        {
            //rs_log(LOG_WARNING, "Missing VALUE on line #%i, %s\n", *line_n, line);
            knode->value = (char *)malloc(strlen("0") + 1);
            sprintf(knode->value, "0");
        }
        else
        {
            knode->value = (char *)malloc(strlen(temp2) + 1);
            strcpy(knode->value, temp2);
            //rs_log(LOG_DEBUG, "Val[%s]\n", knode->value);
        }

        if (!*klast)
            (*clast)->key = *klast = knode;
        else
        {
            (*klast)->next = knode;
            *klast = knode;
        }
    }
    else
    {
        //rs_log(LOG_ERROR, "Invalid config line #%i, %s\n", *line_n, line);
        return -1;
    }

    return 0;
}

/* Reads the config file and constructs a tree */
struct _CFG *cfg_read(const char *filename)
{
    FILE *cf;
    struct _CFG *conf, *clast;
    struct _KEY *klast;
    char line[MAX_LINE_SIZE];
    int line_n;

    conf = clast = NULL;
    klast = NULL;

    cf = fopen(filename, "r");
    if (!cf)
    {
        return (NULL);
    }

    line_n = 0;
    line[0] = '*';        /* this stops the space searchers from going too far */

    while (fgets(&line[1], MAX_LINE_SIZE - 1, cf) != NULL)
    {
        if (cfg_parse_line(&line_n, line, &conf, &clast, &klast) < 0)
        {
            conf = NULL;
            break;
        }
    }
//   if (!conf)
        //rs_log(LOG_ERROR, "Config file \"%s\" is invalid\n", filename);

    fclose(cf);

    return (conf);
}

void cfg_clean(struct _CFG *conf)
{
    struct _CFG *cnode;
    struct _KEY *knode;

    while (conf)
    {
        while (conf->key)
        {
            knode = conf->key;
            conf->key = knode->next;
            free(knode->value);
            free(knode->name);
            free(knode);
        }

        cnode = conf;
        conf = conf->next;
        free(cnode->app);
        free(cnode);
    }
}



char  *cfg_chk_app(const struct _CFG *cnode, const char *app)
{
    while (cnode)
    {
//        rs_log(LOG_DEBUG, "cnode: %s %s\n", cnode->app, app);
        if (!strcmp(app, cnode->app))
            return cnode->app;

        cnode = cnode->next;
    }

    return NULL;
}



struct _KEY *cfg_get_app(const struct _CFG *cnode, const char *app)
{
    while (cnode)
    {
//        rs_log(LOG_DEBUG, "cnode: %s %s\n", cnode->app, app);
        if (!strcmp(app, cnode->app))
            return cnode->key;

        cnode = cnode->next;
    }

    return NULL;
}

char *cfg_get_val_with_knode(const struct _KEY *knode, const char *key_name)
{
    while (knode)
    {
//        rs_log(LOG_DEBUG, "knode: %s %s\n", knode->name, key_name);

        if (!strcmp(key_name, knode->name))
            return knode->value;

        knode = knode->next;
    }

    return NULL;
}

char *cfg_get_val(const struct _CFG *cfg, const char *app, const char *key_name)
{
    struct _KEY *knode;

    knode = cfg_get_app(cfg, app);
    if (knode == NULL)
        return NULL;

    return cfg_get_val_with_knode(knode, key_name);
}

