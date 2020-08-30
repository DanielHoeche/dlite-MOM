#include "config.h"
#include "config-paths.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "utils/err.h"
#include "utils/tgen.h"
#include "utils/plugin.h"

#include "dlite-misc.h"
#include "dlite-datamodel.h"
#include "dlite-storage-plugins.h"


struct _DLiteStoragePluginIter {
  PluginIter iter;
};

/* Global reference to storage plugin info */
static PluginInfo *storage_plugin_info=NULL;


/* Frees up `storage_plugin_info`. */
static void storage_plugin_info_free(void)
{
  if (storage_plugin_info) plugin_info_free(storage_plugin_info);
  storage_plugin_info = NULL;
}

/* Returns a pointer to `storage_plugin_info`. */
static PluginInfo *get_storage_plugin_info(void)
{
  if (!storage_plugin_info &&
      (storage_plugin_info =
       plugin_info_create("storage-plugin",
			  "get_dlite_storage_plugin_api",
			  "DLITE_STORAGE_PLUGIN_DIRS"))) {
    atexit(storage_plugin_info_free);

    if (dlite_use_build_root())
      plugin_path_extend(storage_plugin_info, dlite_STORAGE_PLUGINS, NULL);
    else
      plugin_path_extend_prefix(storage_plugin_info, dlite_root_get(),
                                DLITE_STORAGE_PLUGIN_DIRS, NULL);
  }
  return storage_plugin_info;
}


/*
  Returns a storage plugin with the given name, or NULL if it cannot
  be found.

  If a plugin with the given name is registered, it is returned.

  Otherwise the plugin search path is checked for shared libraries
  matching `name.EXT` where `EXT` is the extension for shared library
  on the current platform ("dll" on Windows and "so" on Unix/Linux).
  If a plugin with the provided name is found, it is loaded,
  registered and returned.

  Otherwise the plugin search path is checked again, but this time for
  any shared library.  If a plugin with the provided name is found, it
  is loaded, registered and returned.

  Otherwise NULL is returned.
 */
const DLiteStoragePlugin *dlite_storage_plugin_get(const char *name)
{
  const DLiteStoragePlugin *api;
  PluginInfo *info;

  if (!(info = get_storage_plugin_info())) return NULL;

  if (!(api = (const DLiteStoragePlugin *)plugin_get_api(info, name))) {
    /* create informative error message... */
    TGenBuf buf;
    int n=0;
    const char *p, **paths = dlite_storage_plugin_paths();
    char *submsg = (dlite_use_build_root()) ? "" : "DLITE_ROOT or ";
    tgen_buf_init(&buf);
    tgen_buf_append_fmt(&buf, "cannot find storage plugin for driver \"%s\" "
                        "in search path:\n", name);
    while ((p = *(paths++)) && ++n) tgen_buf_append_fmt(&buf, "    %s\n", p);
    if (n <= 1)
      tgen_buf_append_fmt(&buf, "Is the %sDLITE_STORAGE_PLUGIN_DIRS "
                          "enveronment variable(s) set?", submsg);

    errx(1, "%s", tgen_buf_get(&buf));
    tgen_buf_deinit(&buf);
  }
  return api;
}

/*
  Registers `api` for a storage plugin.  Returns non-zero on error.
*/
int dlite_storage_plugin_register_api(const DLiteStoragePlugin *api)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_register_api(info, (const PluginAPI *)api);
}

/*
  Load all plugins that can be found in the plugin search path.
  Returns non-zero on error.
 */
int dlite_storage_plugin_load_all()
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  plugin_load_all(info);
  return 0;
}

/*
  Unloads and unregisters all storage plugins.
*/
void dlite_storage_plugin_unload_all()
{
  PluginInfo *info;
  char **p, **names;
  if (!(info = get_storage_plugin_info())) return;
  if (!(names = plugin_names(info))) return;
  for (p=names; *p; p++) {
    plugin_unload(info, *p);
    free(*p);
  }
  free(names);
}

/*
  Returns a pointer to a new plugin iterator or NULL on error.  It
  should be free'ed with dlite_storage_plugin_iter_free().
 */
DLiteStoragePluginIter *dlite_storage_plugin_iter_create()
{
  PluginInfo *info;
  DLiteStoragePluginIter *iter;
  if (!(info = get_storage_plugin_info())) return NULL;
  if (!(iter = calloc(1, sizeof(DLiteStoragePluginIter))))
    return err(1, "allocation failure"), NULL;
  plugin_api_iter_init(&iter->iter, info);
  return iter;
}

/*
  Returns pointer the next plugin or NULL if there a re no more plugins.
  `iter` is the iterator returned by dlite_storage_plugin_iter_create().
 */
const DLiteStoragePlugin *
dlite_storage_plugin_iter_next(DLiteStoragePluginIter *iter)
{
  return (const DLiteStoragePlugin *)plugin_api_iter_next(&iter->iter);
}

/*
  Frees plugin iterator `iter` created with
  dlite_storage_plugin_iter_create().
 */
void dlite_storage_plugin_iter_free(DLiteStoragePluginIter *iter)
{
  free(iter);
}


/*
  Unloads and unregisters storage plugin with the given name.
  Returns non-zero on error.
*/
int dlite_storage_plugin_unload(const char *name)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_unload(info, name);
}


/*
  Returns a NULL-terminated array of pointers to search paths or NULL
  if no search path is defined.

  Use dlite_storage_plugin_path_insert(), dlite_storage_plugin_path_append()
  and dlite_storage_plugin_path_remove() to modify it.
*/
const char **dlite_storage_plugin_paths()
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return NULL;
  return plugin_path_get(info);
}

/*
  Inserts `path` into the current search path at index `n`.  If `n` is
  negative, it counts from the end of the search path (like Python).

  If `n` is out of range, it is clipped.

  Returns non-zero on error.
*/
int dlite_storage_plugin_path_insert(int n, const char *path)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_path_insert(info, path, n);
}

/*
  Appends `path` into the current search path.

  Returns non-zero on error.
*/
int dlite_storage_plugin_path_append(const char *path)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_path_append(info, path);
}

/*
  Like dlite_storage_plugin_path_append(), but appends at most the
  first `n` bytes of `path` to the current search path.

  Returns non-zero on error.
*/
int dlite_storage_plugin_path_appendn(const char *path, size_t n)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_path_appendn(info, path, n);
}

/*
  Removes path number `n` from current search path.

  Returns non-zero on error.
*/
int dlite_storage_plugin_path_remove(int n)
{
  PluginInfo *info;
  if (!(info = get_storage_plugin_info())) return 1;
  return plugin_path_remove(info, n);
}
