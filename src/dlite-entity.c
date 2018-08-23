#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "err.h"
#include "dlite.h"
#include "dlite-type.h"
#include "dlite-entity.h"
#include "dlite-datamodel.h"


/* schema_entity */
static DLiteMeta schema_entity = {
  "00000000-0000-0000-0000-000000000000",     /* uuid */
  "http://meta.sintef.no/0.1/schema-entity",  /* uri  */
  NULL,                                       /* meta */
  "Schema for Entities",                      /* description */
  sizeof(DLiteEntity),                        /* size */
  offsetof(DLiteEntity, dimensions),          /* dimoffset */
  NULL,                                       /* propoffsets */
  0,                                          /* reloffset */
  1,                                          /* refcount, never free */
  NULL,                                       /* dimensions */
  NULL,                                       /* properties */
  NULL,                                       /* relations */
  3,                                          /* ndimensions */
  2,                                          /* nproperties */
  0,                                          /* nrelations */
};


/* Convenient macros for failing */
#define FAIL(msg) do { err(1, msg); goto fail; } while (0)
#define FAIL1(msg, a1) do { err(1, msg, a1); goto fail; } while (0)
#define FAIL2(msg, a1, a2) do { err(1, msg, a1, a2); goto fail; } while (0)


/********************************************************************
 *  Instances
 ********************************************************************/

/* FIXME: allow these functions to also instansiating metadata.
   Should not be too difficult to fix... */

/*
  Returns a new dlite instance from Entiry `meta` and dimensions
  `dims`.  The lengths of `dims` is found in `meta->ndims`.

  The `id` argment may be NULL, a valid UUID or an unique identifier
  to this instance (e.g. an uri).  In the first case, a random UUID
  will be generated. In the second case, the instance will get the
  provided UUID.  In the third case, an UUID will be generated from
  `id`.  In addition, the instanc's uri member will be assigned to
  `id`.

  All properties are initialised to zero and arrays for all dimensional
  properties are allocated and initialised to zero.

  On error, NULL is returned.
 */
DLiteInstance *dlite_instance_create(DLiteEntity *meta, size_t *dims,
                                     const char *id)
{
  char uuid[DLITE_UUID_LENGTH+1];
  size_t i;
  DLiteInstance *inst=NULL;
  int j, uuid_version;

  /* Allocate instance */
  if (!(inst = calloc(1, meta->size))) FAIL("allocation failure");

  /* Initialise header */
  if ((uuid_version = dlite_get_uuid(uuid, id)) < 0) goto fail;
  memcpy(inst->uuid, uuid, sizeof(uuid));
  if (uuid_version == 5) inst->uri = strdup(id);
  inst->meta = (DLiteMeta *)meta;

  /* Set dimensions */
  if (meta->ndimensions) {
    size_t *dimensions = (size_t *)((char *)inst + meta->dimoffset);
    memcpy(dimensions, dims, meta->ndimensions*sizeof(size_t));
  }

  /* Allocate arrays for dimensional properties */
  for (i=0; i<meta->nproperties; i++) {
    DLiteBaseProperty *p = meta->properties[i];
    void **ptr = (void **)((char *)inst + meta->propoffsets[i]);

    if (p->ndims > 0 && p->dims) {
      size_t nmemb=1;
      for (j=0; j<p->ndims; j++)
        nmemb *= dims[p->dims[j]];
      if (!(*ptr = calloc(nmemb, p->size))) goto fail;
    }
  }

  dlite_entity_incref(meta);  /* increase metadata refcount */

  return inst;
 fail:
  if (inst) dlite_instance_free(inst);
  return NULL;
}


/*
  Free's an instance and all arrays associated with dimensional properties.
 */
void dlite_instance_free(DLiteInstance *inst)
{
  DLiteMeta *meta = inst->meta;
  size_t i, nprops = meta->nproperties;

  if (inst->uri) free((char *)inst->uri);
  if (meta->properties) {
    for (i=0; i<nprops; i++) {
      DLiteProperty *p = (DLiteProperty *)meta->properties[i];

      if (p->type == dliteStringPtr) {
          int j;
          size_t n, nmemb=1, *dims=(size_t *)((char *)inst + meta->dimoffset);
          char **ptr = (char **)((char *)inst + meta->propoffsets[i]);
          if (p->ndims > 0 && p->dims) ptr = *((char ***)ptr);
          if (p->dims)
            for (j=0; j<p->ndims; j++) nmemb *= dims[p->dims[j]];
          for (n=0; n<nmemb; n++)
            free((ptr)[n]);
          if (p->ndims > 0 && p->dims)
            free(ptr);
      } else if (p->ndims > 0 && p->dims) {
	void **ptr = (void *)((char *)inst + meta->propoffsets[i]);
        free(*ptr);
      }
    }
  }
  free(inst);

  dlite_meta_decref(meta);  /* decrease metadata refcount */
}


/*
  Loads instance identified by `id` from storage `s` and returns a
  new and fully initialised dlite instance.

  On error, NULL is returned.
 */
DLiteInstance *dlite_instance_load(DLiteStorage *s, const char *id,
				   DLiteEntity *entity)
{
  DLiteInstance *inst, *instance=NULL;
  DLiteDataModel *d=NULL;
  size_t i, *dims=NULL, *pdims=NULL;
  int j, max_pndims=0;
  const char *uri=NULL;

  if (!(d = dlite_datamodel(s, id))) goto fail;

  /* check metadata uri */
  if (!(uri = dlite_datamodel_get_meta_uri(d))) goto fail;
  if (strcmp(uri, entity->uri) != 0)
    FAIL2("metadata (%s) does not correspond to metadata in storage (%s)",
	  entity->uri, uri);

  /* read dimensions */
  if (!(dims = calloc(entity->ndimensions, sizeof(size_t))))
    FAIL("allocation failure");
  for (i=0; i<entity->ndimensions; i++)
    if (!(dims[i] =
	  dlite_datamodel_get_dimension_size(d, entity->dimensions[i].name)))
      goto fail;

  /* create instance */
  if (!(inst = dlite_instance_create(entity, dims, id))) goto fail;

  /* assign properties */
  for (i=0; i<entity->nproperties; i++) {
    DLiteProperty *p = (DLiteProperty *)entity->properties[i];
    if (p->ndims > max_pndims) max_pndims = p->ndims;
  }
  pdims = malloc(max_pndims * sizeof(size_t));
  for (i=0; i<entity->nproperties; i++) {
    DLiteProperty *p = (DLiteProperty *)entity->properties[i];
    void *ptr = (void *)dlite_instance_get_property_by_index(inst, i);
    for (j=0; j<p->ndims; j++) pdims[j] = dims[p->dims[j]];
    if (p->ndims > 0 && p->dims) ptr = *((void **)ptr);
    if (dlite_datamodel_get_property(d, p->name, ptr, p->type, p->size,
				     p->ndims, pdims)) goto fail;
  }

  instance = inst;
 fail:
  if (!instance) {
    if (inst) dlite_instance_free(inst);
  }
  if (d) dlite_datamodel_free(d);
  if (uri) free((char *)uri);
  if (dims) free(dims);
  if (pdims) free(pdims);
  return instance;
}


/*
  Saves instance `inst` to storage `s`.  Returns non-zero on error.
 */
int dlite_instance_save(DLiteStorage *s, const DLiteInstance *inst)
{
  DLiteDataModel *d=NULL;
  DLiteEntity *entity = (DLiteEntity *)inst->meta;
  int j, max_pndims=0, retval=1;
  size_t i, *pdims, *dims;

  if (!(d = dlite_datamodel(s, inst->uuid))) goto fail;

  if (dlite_datamodel_set_meta_uri(d, entity->uri)) goto fail;

  dims = (size_t *)((char *)inst + inst->meta->dimoffset);
  for (i=0; i<entity->ndimensions; i++) {
    char *dimname = inst->meta->dimensions[i].name;
    if (dlite_datamodel_set_dimension_size(d, dimname, dims[i])) goto fail;
  }

  for (i=0; i<entity->nproperties; i++) {
    DLiteProperty *p = (DLiteProperty *)entity->properties[i];
    if (p->ndims > max_pndims) max_pndims = p->ndims;
  }
  pdims = malloc(max_pndims * sizeof(size_t));

  for (i=0; i<entity->nproperties; i++) {
    DLiteProperty *p = (DLiteProperty *)inst->meta->properties[i];
    const void *ptr = dlite_instance_get_property_by_index(inst, i);
    for (j=0; j<p->ndims; j++) pdims[j] = dims[p->dims[j]];
    if (p->ndims > 0 && p->dims) ptr = *((void **)ptr);

    if (dlite_datamodel_set_property(d, p->name, ptr, p->type, p->size,
				     p->ndims, pdims)) goto fail;
  }
  retval = 0;
 fail:
  if (d) dlite_datamodel_free(d);
  if (pdims) free(pdims);
  return retval;
}


/*
  Returns size of dimension `i` or -1 on error.
 */
int dlite_instance_get_dimension_size_by_index(const DLiteInstance *inst,
                                               size_t i)
{
  size_t *dimensions = (size_t *)((char *)inst + inst->meta->dimoffset);
  if (i >= inst->meta->nproperties)
    return errx(-1, "no property with index %lu in %s", i, inst->meta->uri);
  return dimensions[i];
}

/*
  Returns a pointer to data for property `i` or NULL on error.
 */
const void *dlite_instance_get_property_by_index(const DLiteInstance *inst,
						 size_t i)
{
  if (i >= inst->meta->nproperties)
    return errx(1, "no property with index %lu in %s",
		i, inst->meta->uri), NULL;
  return (char *)inst + inst->meta->propoffsets[i];
}

/*
  Copies memory pointed to by `ptr` to property `i`.
  Returns non-zero on error.
*/
int dlite_instance_set_property_by_index(DLiteInstance *inst, size_t i,
					 const void *ptr)
{
  DLiteMeta *meta = inst->meta;
  DLiteBaseProperty *p = meta->properties[i];
  size_t n, nmemb=1, *dims=(size_t *)((char *)inst + meta->dimoffset);
  int j;

  void *dest = (void *)((char *)inst + meta->propoffsets[i]);
  if (p->ndims > 0 && p->dims) dest = *((void **)dest);

  if (p->dims)
    for (j=0; j<p->ndims; j++) nmemb *= dims[p->dims[j]];

  if (p->type == dliteStringPtr) {
    char **q=(char **)dest, **src = (char **)ptr;
    for (n=0; n<nmemb; n++) {
      size_t len = strlen(src[n]) + 1;
      q[n] = realloc(q[n], len);
      memcpy(q[n], src[n], len);
    }
  } else {
    memcpy(dest, ptr, nmemb*p->size);
  }
  return 0;
}

/*
  Returns number of dimensions of property with index `i` or -1 on error.
 */
int dlite_instance_get_property_ndims_by_index(const DLiteInstance *inst,
					       size_t i)
{
  const DLiteProperty *p;
  if (!(p = dlite_entity_get_property_by_index((DLiteEntity *)inst->meta, i)))
    return -1;
  return p->ndims;
}

/*
  Returns size of dimension `j` in property `i` or -1 on error.
 */
int dlite_instance_get_property_dimsize_by_index(const DLiteInstance *inst,
						 size_t i, size_t j)
{
  const DLiteProperty *p;
  size_t *dims = (size_t *)((char *)inst + inst->meta->dimoffset);
  if (!(p = dlite_entity_get_property_by_index((DLiteEntity *)inst->meta, i)))
    return -1;
  if (j >= (size_t)p->ndims)
    return errx(-1, "dimension index j=%lu is our of range", j);
  return dims[p->dims[j]];
}

/*
  Returns size of dimension `i` or -1 on error.
 */
int dlite_instance_get_dimension_size(const DLiteInstance *inst,
                                      const char *name)
{
  int i;
  size_t *dimensions = (size_t *)((char *)inst + inst->meta->dimoffset);
  if ((i = dlite_meta_get_dimension_index(inst->meta, name)) < 0) return -1;
  if (i >= (int)inst->meta->nproperties)
    return errx(-1, "no property with index %d in %s", i, inst->meta->uri);
  return dimensions[i];
}

/*
  Returns a pointer to data corresponding to `name` or NULL on error.
 */
const void *dlite_instance_get_property(const DLiteInstance *inst,
					const char *name)
{
  int i;
  if ((i = dlite_meta_get_property_index(inst->meta, name)) < 0) return NULL;
  return (char *)inst + inst->meta->propoffsets[i];
}

/*
  Copies memory pointed to by `ptr` to property `name`.
  Returns non-zero on error.
*/
int dlite_instance_set_property(DLiteInstance *inst, const char *name,
				const void *ptr)
{
  int i;
  if ((i = dlite_meta_get_property_index(inst->meta, name)) < 0) return 1;
  return dlite_instance_set_property_by_index(inst, i, ptr);
}

/*
  Returns number of dimensions of property  `name` or -1 on error.
*/
int dlite_instance_get_property_ndims(const DLiteInstance *inst,
				      const char *name)
{
  const DLiteProperty *p;
  if (!(p = dlite_entity_get_property((DLiteEntity *)inst->meta, name)))
    return -1;
  return p->ndims;
}

/*
  Returns size of dimension `j` of property `name` or NULL on error.
*/
size_t dlite_instance_get_property_dimssize(const DLiteInstance *inst,
					    const char *name, size_t j)
{
  int i;
  if ((i = dlite_meta_get_property_index(inst->meta, name)) < 0) return -1;
  return dlite_instance_get_property_dimsize_by_index(inst, i, j);
}


/********************************************************************
 *  Entities
 ********************************************************************/

/*
  Returns a new Entity created from the given arguments.
 */
DLiteEntity *
dlite_entity_create(const char *uri, const char *description,
		    size_t ndimensions, const DLiteDimension *dimensions,
		    size_t nproperties, const DLiteProperty *properties)
{
  DLiteEntity *entity=NULL;
  char uuid[DLITE_UUID_LENGTH+1];
  int uuid_version;

  if (!(entity = calloc(1, sizeof(DLiteEntity)))) FAIL("allocation error");

  if ((uuid_version = dlite_get_uuid(uuid, uri)) < 0) goto fail;
  memcpy(entity->uuid, uuid, sizeof(uuid));
  if (uuid_version == 5) entity->uri = strdup(uri);
  entity->meta = &schema_entity;
  if (description) entity->description = strdup(description);

  if (ndimensions) {
    if (!(entity->dimensions = calloc(ndimensions, sizeof(DLiteDimension))))
      FAIL("allocation error");
    memcpy(entity->dimensions, dimensions, ndimensions*sizeof(DLiteDimension));
  }

  if (nproperties) {
    size_t i, propsize = nproperties * sizeof(DLiteProperty *);
    if (!(entity->properties = malloc(propsize)))
      FAIL("allocation error");
    for (i=0; i<nproperties; i++) {
      DLiteProperty *p;
      const DLiteProperty *q = properties + i;
      if (!(p = calloc(1, sizeof(DLiteProperty))))
        FAIL("allocation error");
      memcpy(p, q, sizeof(DLiteProperty));
      if (!(p->name = strdup(q->name)))
        FAIL("allocation error");
      if (p->ndims) {
        if (!(p->dims = malloc(p->ndims*sizeof(int))))
          FAIL("allocation error");
        memcpy(p->dims, properties[i].dims, p->ndims*sizeof(int));
      } else {
        p->dims = NULL;
      }
      if (q->description && !(p->description = strdup(q->description)))
        FAIL("allocation error");
      if (q->unit && !(p->unit = strdup(q->unit)))
        FAIL("allocation error");
      entity->properties[i] = (DLiteBaseProperty *)p;
    }
  }
  entity->ndimensions = ndimensions;
  entity->nproperties = nproperties;

  if (dlite_meta_postinit((DLiteMeta *)entity, 0)) goto fail;

  /* Set refcount to 1, since we return a reference to `entity`.
     Also increase the reference count to the meta-metadata. */
  entity->refcount = 1;
  dlite_meta_incref(entity->meta);

  return entity;
 fail:
  if (entity) {
    dlite_entity_clear(entity);
    free(entity);
  }
  return NULL;
}

/*
  Increase reference count to Entity.
 */
void dlite_entity_incref(DLiteEntity *entity)
{
  dlite_meta_incref((DLiteMeta *)entity);
}

/*
  Decrease reference count to Entity.  If the reference count reaches
  zero, the Entity is free'ed.
 */
void dlite_entity_decref(DLiteEntity *entity)
{
  if (entity) {
    if (--entity->refcount <= 0) {
      if (entity->meta) dlite_meta_decref(entity->meta);
      dlite_entity_clear(entity);
      free(entity);
    }
  }
}

/*
  Free's all memory used by `entity` and clear all data.
 */
void dlite_entity_clear(DLiteEntity *entity)
{
  size_t i;
  for (i=0; i<entity->nproperties; i++) {
    DLiteProperty *p = (DLiteProperty *)entity->properties[i];
    if (p->unit) {
      free(p->unit);
      p->unit = NULL;
    }
  }
  dlite_meta_clear((DLiteMeta *)entity);
}


/*
  Returns a new Entity loaded from storage `s`.  The `id` may be either
  an URI to the Entity (typically of the form "namespace/version/name")
  or an UUID.

  Returns NULL on error.
 */
DLiteEntity *dlite_entity_load(const DLiteStorage *s, const char *id)
{
  char uuid[DLITE_UUID_LENGTH+1];
  int uuidver;

  if (!s->api->getEntity)
    return errx(1, "driver '%s' does not support getEntity()",
                s->api->name), NULL;

  if ((uuidver = dlite_get_uuid(uuid, id)) != 0 || uuidver != 5)
    return errx(1, "id '%s' is not an UUID or a string that we can generate an uuid from", id), NULL;
  return s->api->getEntity(s, uuid);
}

/*
  Saves an Entity to storage `s`.  Returns non-zero on error.
 */
int dlite_entity_save(DLiteStorage *s, const DLiteEntity *e)
{
  if (!s->api->setEntity)
    return errx(1, "driver '%s' does not support setEntity()",
                s->api->name);

  return s->api->setEntity(s, e);
}


/*
  Returns a pointer to property with index `i` or NULL on error.
 */
const DLiteProperty *
dlite_entity_get_property_by_index(const DLiteEntity *entity, size_t i)
{
  if (i >= entity->nproperties)
    return errx(1, "no property with index %lu in %s", i , entity->meta->uri),
      NULL;
  return (const DLiteProperty *)entity->properties[i];
}

/*
  Returns a pointer to property named `name` or NULL on error.
 */
const DLiteProperty *dlite_entity_get_property(const DLiteEntity *entity,
					       const char *name)
{
  int i;
  if ((i = dlite_meta_get_property_index((DLiteMeta *)entity, name)) < 0)
    return NULL;
  return (const DLiteProperty *)entity->properties[i];
}


/********************************************************************
 *  Meta data
 *
 *  These functions are mainly used internally or by code generators.
 *  Do not waist time on them...
 ********************************************************************/

/*
  Initialises internal data of `meta`.  This function should not be
  called before the non-internal properties has been initialised.

  The `ismeta` argument indicates whether the instance described by
  `meta` is metadata itself.

  Returns non-zero on error.
 */
int dlite_meta_postinit(DLiteMeta *meta, bool ismeta)
{
  size_t maxalign;     /* max alignment of any member */
  int retval=1;

  assert(meta->meta);

  if (ismeta) {
    /* Metadata

       Since we hardcode `ndimensions`, `nproperties` and `nrelations` in
       DLiteMeta_HEAD, there will allways be at least 3 dimensions... */
    assert(meta->meta->ndimensions >= 3);
    meta->size = sizeof(DLiteMeta) +
      (meta->meta->ndimensions - 3)*sizeof(size_t);
    meta->dimoffset = offsetof(DLiteMeta, dimensions);
    meta->propoffsets = NULL;
    meta->reloffset = offsetof(DLiteMeta, relations);
  } else {
    /* Instance */
    DLiteType proptype;
    size_t i, align, propsize, padding, offset, size;

    if (!(meta->propoffsets = calloc(meta->nproperties, sizeof(size_t))))
      FAIL("allocation failure");

    /* -- header */
    offset = offsetof(DLiteInstance, meta);
    size = sizeof(DLiteMeta *);
    if (!(maxalign = dlite_type_get_alignment(dliteStringPtr, sizeof(char *))))
      goto fail;

    /* -- dimensions */
    for (i=0; i<meta->ndimensions; i++) {
      offset = dlite_type_get_member_offset(offset, size,
					    dliteInt, sizeof(size_t));
      size = sizeof(int);
      if (i == 0) meta->dimoffset = offset;
    }
    if (meta->ndimensions &&
	(align = dlite_type_get_alignment(dliteUInt,
					  sizeof(size_t))) > maxalign)
      maxalign = align;

    /* -- properties */
    for (i=0; i<meta->nproperties; i++) {
      DLiteBaseProperty *p = *(meta->properties + i);
      if (p->ndims > 0 && p->dims) {
	proptype = dliteBlob;       /* pointer */
	propsize = sizeof(void *);
      } else {
	proptype = p->type;
	propsize = p->size;
      }
      offset = dlite_type_get_member_offset(offset, size, proptype, propsize);
      size = propsize;
      meta->propoffsets[i] = offset;

      if ((align = dlite_type_get_alignment(proptype, propsize)) > maxalign)
	maxalign = align;
    }

    /* -- relations */
    for (i=0; i<meta->nrelations; i++) {
      offset = dlite_type_get_member_offset(offset, size, dliteStringPtr,
					    sizeof(DLiteTriplet *));
      size = sizeof(DLiteTriplet *);
    }
    meta->reloffset = offset;

    offset += size;
    padding = (maxalign - (offset & (maxalign - 1))) & (maxalign - 1);
    meta->size = offset + padding;
  }

  retval = 0;
 fail:
  return retval;
}


/*
  Free's all memory used by `meta` and clear all data.
 */
void dlite_meta_clear(DLiteMeta *meta)
{
  size_t i;
  if (meta->uri) free((char *)meta->uri);
  if (meta->description) free((char *)meta->description);

  if (meta->propoffsets) free(meta->propoffsets);

  if (meta->dimensions) free(meta->dimensions);
  if (meta->properties) {
    for (i=0; i<meta->nproperties; i++) {
      DLiteBaseProperty *p = meta->properties[i];
      if (p) {
        if (p->name) free(p->name);
        if (p->dims) free(p->dims);
        if (p->description) free(p->description);
        free(p);
      }
    }
    free(meta->properties);
  }
  if (meta->relations) free(meta->relations);

  memset(meta, 0, sizeof(DLiteMeta));
}

/*
  Increase reference count to meta-metadata.
 */
void dlite_meta_incref(DLiteMeta *meta)
{
  if (meta) meta->refcount++;
}

/*
  Decrease reference count to meta-metadata.  If the reference count reaches
  zero, the meta-metadata is free'ed.
 */
void dlite_meta_decref(DLiteMeta *meta)
{
  if (meta) {
    if (--meta->refcount <= 0) {
      if (meta->meta) dlite_meta_decref(meta->meta);  /* decrease refcount */
      dlite_meta_clear(meta);
      free(meta);
    }
  }
}


/*
  Returns index of dimension named `name` or -1 on error.
 */
int dlite_meta_get_dimension_index(const DLiteMeta *meta, const char *name)
{
  size_t i;
  for (i=0; i<meta->ndimensions; i++) {
    DLiteDimension *p = meta->dimensions + i;
    if (strcmp(name, p->name) == 0) return i;
  }
  return err(-1, "%s has no such dimension: '%s'", meta->uri, name);
}

/*
  Returns index of property named `name` or -1 on error.
 */
int dlite_meta_get_property_index(const DLiteMeta *meta, const char *name)
{
  size_t i;
  for (i=0; i<meta->nproperties; i++) {
    DLiteBaseProperty *p = meta->properties[i];
    if (strcmp(name, p->name) == 0) return i;
  }
  return err(-1, "%s has no such property: '%s'", meta->uri, name);
}

/*
  Returns a pointer to property named `name` or NULL on error.
 */
const DLiteBaseProperty *dlite_meta_get_property(const DLiteMeta *meta,
						 const char *name)
{
  int i;
  if ((i = dlite_meta_get_property_index(meta, name)) < 0) return NULL;
  return meta->properties[i];
}