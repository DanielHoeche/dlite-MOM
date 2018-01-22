#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"
#include "dlite.h"
#include "dlite-datamodel.h"
#include "err.h"


/* Convenient macros for failing */
#define FAIL(msg) do { err(1, msg); goto fail; } while (0)
#define FAIL1(msg, a1) do { err(1, msg, a1); goto fail; } while (0)



/********************************************************************
 * Required api
 ********************************************************************/


/*
  Returns a new data model for instance `id` in storage `s` or NULL on error.
  Should be free'ed with dlite_datamodel_free().
 */
DLiteDataModel *dlite_datamodel(const DLiteStorage *s, const char *id)
{
  DLiteDataModel *d;
  char uuid[DLITE_UUID_LENGTH+1];
  int uuidver;

  if ((uuidver = dlite_get_uuid(uuid, id)) < 0)
    return err(1, "failed generating UUID from id \"%s\"", id), NULL;

  if (!(d = s->api->dataModel(s, uuid)))
    return err(1, "cannot create datamodel id='%s' for storage '%s'",
               id, s->api->name), NULL;

  /* Initialise common fields */
  d->api = s->api;  /* remove this field? */
  d->s = (DLiteStorage *)s;
  memcpy(d->uuid, uuid, sizeof(d->uuid));

  if (uuidver == 5 && s->writable && s->api->setDataName)
    s->api->setDataName(d, id);

  return d;
}


/*
  Clears a data model initialised with dlite_datamodel_init().
 */
int dlite_datamodel_free(DLiteDataModel *d)
{
  int stat=0;
  assert(d);
  if (d->api->dataModelFree) stat = d->api->dataModelFree(d);
  free(d);
  return stat;
}


/*
  Returns pointer to metadata or NULL on error. Do not free.
 */
const char *dlite_datamodel_get_metadata(const DLiteDataModel *d)
{
  return d->api->getMetadata(d);
}


/*
  Returns the size of dimension `name` or -1 on error.
 */
int dlite_datamodel_get_dimension_size(const DLiteDataModel *d,
                                       const char *name)
{
  return d->api->getDimensionSize(d, name);
}


/*
  Copies property `name` to memory pointed to by `ptr`.  Max `size`
  bytes are written.  If `size` is too small (or `ptr` is NULL),
  nothing is copied to `ptr`.

  Returns non-zero on error.
 */
int dlite_datamodel_get_property(const DLiteDataModel *d, const char *name,
                                 void *ptr, DLiteType type, size_t size,
                                 int ndims, const int *dims)
{
  return d->api->getProperty(d, name, ptr, type, size, ndims, dims);
}


/********************************************************************
 * Optional api
 ********************************************************************/

/*
  Sets property `name` to the memory (of `size` bytes) pointed to by `value`.
  Returns non-zero on error.
*/
int dlite_datamodel_set_property(DLiteDataModel *d, const char *name,
                                 const void *ptr, DLiteType type, size_t size,
                                 int ndims, const int *dims)
{
  if (!d->api->setProperty)
    return errx(1, "driver '%s' does not support set_property", d->api->name);
  return d->api->setProperty(d, name, ptr, type, size, ndims, dims);
}


/*
  Sets metadata.  Returns non-zero on error.
 */
int dlite_datamodel_set_metadata(DLiteDataModel *d, const char *metadata)
{
  if (!d->api->setMetadata)
    return errx(1, "driver '%s' does not support set_metadata", d->api->name);
  return d->api->setMetadata(d, metadata);
}


/*
  Sets size of dimension `name`.  Returns non-zero on error.
*/
int dlite_datamodel_set_dimension_size(DLiteDataModel *d, const char *name,
                                       int size)
{
  if (!d->api->setDimensionSize)
    return errx(1, "driver '%s' does not support set_dimension_size",
                d->api->name);
  return d->api->setDimensionSize(d, name, size);
}



/*
  Returns a positive value if dimension `name` is defined, zero if it
  isn't and a negative value on error (e.g. if this function isn't
  supported by the backend).
 */
int dlite_datamodel_has_dimension(DLiteDataModel *d, const char *name)
{
  if (d->api->hasDimension) return d->api->hasDimension(d, name);
  return errx(-1, "driver '%s' does not support hasDimension()", d->api->name);
}


/*
  Returns a positive value if property `name` is defined, zero if it
  isn't and a negative value on error (e.g. if this function isn't
  supported by the backend).
 */
int dlite_datamodel_has_property(DLiteDataModel *d, const char *name)
{
  if (d->api->hasProperty) return d->api->hasProperty(d, name);
  return errx(-1, "driver '%s' does not support hasProperty()", d->api->name);
}


/*
  If the uuid was generated from a unique name, return a pointer to a
  newly malloc'ed string with this name.  Otherwise NULL is returned.
*/
char *dlite_datamodel_get_dataname(DLiteDataModel *d)
{
  if (d->api->getDataName) return d->api->getDataName(d);
  errx(1, "driver '%s' does not support getDataName()", d->api->name);
  return NULL;
}



/********************************************************************
 * Utility functions intended to be used by the storage plugins
 ********************************************************************/

/* Copies data from nested pointer to pointers array \a src to the
   flat continuous C-ordered array \a dst. The size of dest must be
   sufficient large.  Returns non-zero on error. */
int dlite_copy_to_flat(void *dst, const void *src, size_t size, int ndims,
                       const int *dims)
{
  int i, n=0, ntot=1, *ind=NULL, retval=1;
  char *q=dst;
  void **p=(void **)src;

  if (!(ind = calloc(ndims, sizeof(int)))) FAIL("allocation failure");

  for (i=0; i<ndims-1; i++) p = p[ind[i]];
  for (i=0; i<ndims; i++) ntot *= (dims) ? dims[i] : 1;

  while (n++ < ntot) {
    memcpy(q, *p, size);
    p++;
    q += size;
    if (++ind[ndims-1] >= ((dims) ? dims[ndims-1] : 1)) {
      ind[ndims-1] = 0;
      for (i=ndims-2; i>=0; i--) {
        if (++ind[i] < ((dims) ? dims[i] : i))
          break;
        else
          ind[i] = 0;
      }
      for (i=0, p=(void **)src; i<ndims-1; i++) p = p[ind[i]];
    }
  }
  retval = 0;
 fail:
  if (ind) free(ind);
  return retval;
}


/* Copies data from flat continuous C-ordered array \a dst to nested
   pointer to pointers array \a src. The size of dest must be
   sufficient large.  Returns non-zero on error. */
int dlite_copy_to_nested(void *dst, const void *src, size_t size, int ndims,
                         const int *dims)
{
  int i, n=0, ntot=1, *ind=NULL, retval=1;
  const char *q=src;
  void **p=dst;

  if (!(ind = calloc(ndims, sizeof(int)))) FAIL("allocation failure");

  for (i=0; i<ndims-1; i++) p = p[ind[i]];
  for (i=0; i<ndims; i++) ntot *= (dims) ? dims[i] : 1;

  while (n++ < ntot) {
    memcpy(*p, q, size);
    p++;
    q += size;
    if (++ind[ndims-1] >= ((dims) ? dims[ndims-1] : 1)) {
      ind[ndims-1] = 0;
      for (i=ndims-2; i>=0; i--) {
        if (++ind[i] < ((dims) ? dims[i] : i))
          break;
        else
          ind[i] = 0;
      }
      for (i=0, p=(void **)src; i<ndims-1; i++) p = p[ind[i]];
    }
  }
  retval = 0;
 fail:
  if (ind) free(ind);
  return retval;
}
