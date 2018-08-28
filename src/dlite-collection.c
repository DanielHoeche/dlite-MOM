#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "err.h"
#include "dlite-entity.h"
#include "dlite-collection.h"


/* Convenient macros for failing */
#define FAIL(msg) do { err(1, msg); goto fail; } while (0)
#define FAIL1(msg, a1) do { err(1, msg, a1); goto fail; } while (0)
#define FAIL2(msg, a1, a2) do { err(1, msg, a1, a2); goto fail; } while (0)


/*
  Returns a new collection with given id.  If `id` is NULL, a new
  random uuid is generated.

  Returns NULL on error.
 */
DLiteCollection *dlite_collection_create(const char *id)
{
  DLiteCollection *coll=NULL;
  int uuid_version;

  /* Allocate instance */
  if (!(coll = calloc(1, sizeof(DLiteCollection)))) FAIL("allocation failure");

  /* Initialise header */
  if ((uuid_version = dlite_get_uuid(coll->uuid, id)) < 0) goto fail;
  if (uuid_version == 5) coll->uri = strdup(id);
  coll->meta = NULL;  /* FIXME */

  /* Initialise tripletstore

     Note that DLiteTriplet corresponds to XTriplet (including id),
     which is used internally be triplestore. This allows us to
     expose the triplets, while they are managed by the tripletstore.

     However, since store->triplets may be reallocated when new
     relations are added or removed, we have to update triplets.

     Is this really a good idea?
  */
  coll->store = triplestore_create();
  coll->triplets = coll->store->triplets;

  return coll;
 fail:
  if (coll) dlite_collection_free(coll);
  return NULL;
}


/*
  Free's a collection and decreases the reference count of the
  associated metadata.
 */
void dlite_collection_free(DLiteCollection *coll)
{
  size_t i;
  if (coll->uri) free((char *)coll->uri);
  triplestore_free(coll->store);
  if (coll->ndims) {
    for (i=0; i<coll->ndims; i++)
      free(coll->dimnames[i]);
    free(coll->dimnames);
    free(coll->dimsizes);
  }
  if (coll->meta) dlite_meta_decref(coll->meta);
  free(coll);
}


/*
  Adds subject-predicate-object relation to collection.  Returns non-zero
  on error.
 */
int dlite_collection_add_relation(DLiteCollection *coll, const char *s,
                                  const char *p, const char *o)
{
  triplestore_add(coll->store, s, p, o);
  coll->triplets = coll->store->triplets;
  return 0;
}


/*
  Remove matching relations.  Any of `s`, `p` or `o` may be NULL, allowing for
  multiple matches.  Returns the number of relations removed, or -1 on error.
 */
int dlite_collection_remove_relations(DLiteCollection *coll, const char *s,
                                      const char *p, const char *o)
{
  int retval = triplestore_remove(coll->store, s, p, o);
  coll->triplets = coll->store->triplets;
  return retval;
}


/*
  Adds (reference to) instance `inst` to collection.  Returns non-zero on
  error.
 */
int dlite_collection_add(DLiteCollection *coll, const char *label,
                         const DLiteInstance *inst)
{
  if (!inst->meta)
    return err(1, "instance must have associated metadata to be added "
               "to a collection");
  dlite_collection_add_relation(coll, label, "_is-a", "Instance");
  dlite_collection_add_relation(coll, label, "_has-uuid", inst->uuid);
  dlite_collection_add_relation(coll, label, "_has-meta", inst->meta->uri);
  return 0;
}


/*
  Returns instance with given label from collection or NULL on error.
 */
//DLiteInstance *dlite_collection_get(DLiteCollection *coll, const char *label)
//{
//  // FIXME
//  return NULL;
//}


/*
  Removes instance with given label from collection.  Returns non-zero on
  error.
 */
int dlite_collection_remove(DLiteCollection *coll, const char *label)
{
  if (dlite_collection_remove_relations(coll, label, "_is-a", "Instance") > 0) {
    DLiteCollectionState state;
    const DLiteTriplet *t;
    dlite_collection_init_state(coll, &state);
    while ((t=dlite_collection_find(coll,&state, label, "_has-dimmap", NULL)))
      triplestore_remove_by_id(coll->store, t->o);
    dlite_collection_remove_relations(coll, label, "_has-uuid", NULL);
    dlite_collection_remove_relations(coll, label, "_has-meta", NULL);
    dlite_collection_remove_relations(coll, label, "_has-dimmap", NULL);
    return 0;
  }
  return 1;
}


/*
  Initiates a DLiteCollectionState for dlite_collection_find().
*/
void dlite_collection_init_state(const DLiteCollection *coll,
                                 DLiteCollectionState *state)
{
  triplestore_init_state(coll->store, (TripleState *)state);
}


/*
  Finds matching relations.

  If `state` is NULL, only the first match will be returned.

  Otherwise, this function should be called iteratively.  Before the
  first call it should be provided a `state` initialised with
  dlite_collection_init_state().

  For each call it will return a pointer to triplet matching `s`, `p`
  and `o`.  Any of these may be NULL, allowing for multiple matches.
  When no more matches can be found, NULL is returned.

  No other calls to dlite_collection_add(), dlite_collection_find() or
  dlite_collection_add_relation() should be done while searching.
 */
const DLiteTriplet *dlite_collection_find(const DLiteCollection *coll,
                                     DLiteCollectionState *state,
                                     const char *s, const char *p,
                                     const char *o)
{
  if (state)
    return (DLiteTriplet *)triplestore_find(coll->store, (TripleState *)state,
                                            s, p, o);
  else
    return (DLiteTriplet *)triplestore_find_first(coll->store, s, p, o);
}