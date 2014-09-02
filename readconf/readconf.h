/*

*/
#ifndef READCONF_H_
#define READCONF_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif


struct _KEY
{
    char *name;
    char *value;
    struct _KEY *next;
};

struct _CFG
{
    char *app;
    struct _KEY *key;
    struct _CFG *next;
};

struct _CFG    *cfg_read(const char *filename);
void cfg_clean(struct _CFG *conf);
char *cfg_chk_app(const struct _CFG *cnode, const char *app);
struct _KEY *cfg_get_app(const struct _CFG *cnode, const char *app);
char *cfg_get_val(const struct _CFG *cfg, const char *app, const char *key_name);
char *cfg_get_val_with_knode(const struct _KEY *knode, const char *key_name);


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif  /*  READCONF_H_ */

