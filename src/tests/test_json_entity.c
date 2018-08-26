#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "minunit/minunit.h"
#include "dlite.h"

#include "config.h"

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) # s


DLiteEntity *entity = NULL;
DLiteInstance *inst = NULL;


/***************************************************************
 * Test entity
 ***************************************************************/

MU_TEST(test_entity_load)
{
  DLiteStorage *s;
  DLiteInstance *e;  /* the entity cast to a DLiteInstance */
  char *path = STRINGIFY(DLITE_ROOT) "/tools/tests/Chemistry-0.1.json";
  char *uri = "http://www.sintef.no/calm/0.1/Chemistry";

  mu_check((s = dlite_storage_open("json", path, "r")));
  mu_check((entity = dlite_entity_load(s, uri)));
  mu_check(!dlite_storage_close(s));

  e = (DLiteInstance *)entity;

  mu_assert_int_eq(2, dlite_instance_get_dimension_size(e, "ndimensions"));
  mu_assert_int_eq(8, dlite_instance_get_dimension_size(e, "nproperties"));
  /*
  mu_assert_string_eq(
  dlite_instance_get_property((*DLiteInstance)entity,
  */
}


/* */
MU_TEST(test_entity_property)
{
  const DLiteProperty *prop;
  const char *descr;

  descr = "Chemical symbol of each chemical element.  By convension the "
    "dependent element (e.g. Al) is listed first.";
  mu_check((prop = dlite_entity_get_property(entity, "elements")));
  mu_assert_string_eq("elements", prop->name);
  mu_assert_int_eq(dliteStringPtr, prop->type);
  mu_assert_int_eq(sizeof(char *), prop->size);
  mu_assert_int_eq(1, prop->ndims);
  mu_assert_string_eq(descr, prop->description);
}


MU_TEST(test_instance_create)
{
  size_t dims[] = {3, 2};

  char *alloy = "6063";
  char *elements[] = {"Al", "Mg", "Si"};
  char *phases[] = {"beta\"", "beta'"};
  double X0[] = {0.99, 0.005, 0.005};
  double Xp[2][3] = {
    {2./11., 5./11., 4./11.},
    {0.,     9./14., 5./14.}
  };
  double volfrac[] = {0.005, 0.001};
  double rpart[] = {7e-9, 15e-9};
  double atvol[] = {1.9e-29, 1.8e-29};

  DLiteStorage *s;

  inst = dlite_instance_create(entity, dims, "myinst");

  mu_check(!dlite_instance_set_property(inst, "alloy", &alloy));
  mu_check(!dlite_instance_set_property(inst, "elements", elements));
  mu_check(!dlite_instance_set_property(inst, "phases", phases));
  mu_check(!dlite_instance_set_property(inst, "X0", X0));
  mu_check(!dlite_instance_set_property(inst, "Xp", Xp));
  mu_check(!dlite_instance_set_property(inst, "volfrac", volfrac));
  mu_check(!dlite_instance_set_property(inst, "rpart", rpart));
  mu_check(!dlite_instance_set_property(inst, "atvol", atvol));

  mu_check((s = dlite_storage_open("json", "alloys.json", "w")));
  mu_check(!dlite_instance_save(s, inst));
  mu_check(!dlite_storage_close(s));
}

/*


MU_TEST(test_instance_load)
{

  int i;
  char *elements[] = {"Al", "Mg", "Si"};
  int nelements = dlite_instance_get_dimension_size(inst, "nelements");
  const char **data = (const char **)dlite_instance_get_property(inst, "elements");

  mu_assert_int_eq(3, nelements);

  mu_check(data);
  for (i=0; i<nelements; i++) {
    mu_check(data[i]);
    mu_assert_string_eq(elements[i], data[i]);
  }

}
*/

MU_TEST(test_instance_free)
{
  dlite_instance_free(inst);
}


MU_TEST(test_entity_free)
{
  dlite_entity_decref(entity);
}


/***********************************************************************/

MU_TEST_SUITE(test_suite)
{
  MU_RUN_TEST(test_entity_load);     /* setup */
  MU_RUN_TEST(test_entity_property);

  MU_RUN_TEST(test_instance_create);
  /* MU_RUN_TEST(test_instance_save); */
  MU_RUN_TEST(test_instance_free);

  MU_RUN_TEST(test_entity_free);     /* tear down */
}



int main()
{
  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  return (minunit_fail) ? 1 : 0;
}
