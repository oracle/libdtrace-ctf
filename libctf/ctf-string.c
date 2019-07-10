/* CTF string table management.
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <string.h>

/* Convert an encoded CTF string name into a pointer to a C string by looking
  up the appropriate string table buffer and then adding the offset.  */
const char *
ctf_strraw (ctf_file_t *fp, uint32_t name)
{
  ctf_strs_t *ctsp = &fp->ctf_str[CTF_NAME_STID (name)];

  if (ctsp->cts_strs != NULL && CTF_NAME_OFFSET (name) < ctsp->cts_len)
    return (ctsp->cts_strs + CTF_NAME_OFFSET (name));

  /* String table not loaded or corrupt offset.  */
  return NULL;
}

/* Return a guaranteed-non-NULL pointer to the string with the given CTF
   name.  */
const char *
ctf_strptr (ctf_file_t *fp, uint32_t name)
{
  const char *s = ctf_strraw (fp, name);
  return (s != NULL ? s : "(?)");
}

/* Remove all refs to a given atom.  */
static void
ctf_str_purge_atom_refs (ctf_str_atom_t *atom)
{
  ctf_str_atom_ref_t *ref, *next;

  for (ref = ctf_list_next (&atom->csa_refs); ref != NULL; ref = next)
    {
      next = ctf_list_next (ref);
      ctf_list_delete (&atom->csa_refs, ref);
      ctf_free (ref);
    }
}

/* Free an atom (only called on ctf_close().)  */
static void
ctf_str_free_atom (void *a)
{
  ctf_str_atom_t *atom = a;

  ctf_str_purge_atom_refs (atom);
  ctf_free (atom);
}

/* Create the atoms table.  There is always at least one atom in it, the null
   string.  */
int
ctf_str_create_atoms (ctf_file_t *fp)
{
  fp->ctf_str_atoms = ctf_dynhash_create (ctf_hash_string, ctf_hash_eq_string,
					  ctf_free, ctf_str_free_atom);
  if (fp->ctf_str_atoms == NULL)
    return -ENOMEM;

  ctf_str_add (fp, "");
  return 0;
}

/* Destroy the atoms table.  */
void
ctf_str_free_atoms (ctf_file_t *fp)
{
  ctf_dynhash_destroy (fp->ctf_str_atoms);
}

/* Add a string to the atoms table, copying the passed-in string.  Return the
   atom added. Return NULL only when out of memory (and do not touch the
   passed-in string in that case).  Possibly augment the ref list with the
   passed-in ref.  */
static ctf_str_atom_t *
ctf_str_add_ref_internal (ctf_file_t *fp, const char *str,
			  int add_ref, uint32_t *ref)
{
  char *newstr = NULL;
  ctf_str_atom_t *atom = NULL;
  ctf_str_atom_ref_t *aref = NULL;

  atom = ctf_dynhash_lookup (fp->ctf_str_atoms, str);

  if (add_ref)
    {
      if ((aref = ctf_alloc (sizeof (struct ctf_str_atom_ref))) == NULL)
	return NULL;
      aref->caf_ref = ref;
    }

  if (atom)
    {
      if (add_ref)
	{
	  ctf_list_append (&atom->csa_refs, aref);
	  fp->ctf_str_num_refs++;
	}
      return atom;
    }

  if ((atom = ctf_alloc (sizeof (struct ctf_str_atom))) == NULL)
    goto oom;
  memset (atom, 0, sizeof (struct ctf_str_atom));

  if ((newstr = ctf_strdup (str)) == NULL)
    goto oom;

  if (ctf_dynhash_insert (fp->ctf_str_atoms, newstr, atom) < 0)
    goto oom;

  atom->csa_str = newstr;
  atom->csa_snapshot_id = fp->ctf_snapshots;
  if (add_ref)
    {
      ctf_list_append (&atom->csa_refs, aref);
      fp->ctf_str_num_refs++;
    }
  return atom;

 oom:
  ctf_free (atom);
  ctf_free (aref);
  ctf_free (newstr);
  return NULL;
}

/* Add a string to the atoms table and return it, without augmenting the ref
   list for this string.  */
const char *
ctf_str_add (ctf_file_t *fp, const char *str)
{
  ctf_str_atom_t *atom;
  if (!str)
    return NULL;

  atom = ctf_str_add_ref_internal (fp, str, FALSE, 0);
  if (!atom)
    return NULL;

  return atom->csa_str;
}

/* Like ctf_str_add(), but additionally augment the atom's refs list with the
   passed-in ref, whether or not the string is already present.  There is no
   attempt to deduplicate the refs list (but duplicates are harmless).  */
const char *
ctf_str_add_ref (ctf_file_t *fp, const char *str, uint32_t *ref)
{
  ctf_str_atom_t *atom;
  if (!str)
    return NULL;

  atom = ctf_str_add_ref_internal (fp, str, TRUE, ref);
  if (!atom)
    return NULL;

  return atom->csa_str;
}

/* Add an external strtab reference at OFFSET.  */
const char *
ctf_str_add_external (ctf_file_t *fp, const char *str, uint32_t offset)
{
  ctf_str_atom_t *atom;
  if (!str)
    return NULL;

  atom = ctf_str_add_ref_internal (fp, str, FALSE, 0);
  if (!atom)
    return NULL;

  atom->csa_offset = CTF_SET_STID (offset, CTF_STRTAB_1);
  return atom->csa_str;
}

/* A ctf_dynhash_iter_remove() callback that removes atoms later than a given
   snapshot ID.  */
static int
ctf_str_rollback_atom (void *key _libctf_unused_, void *value, void *arg)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;
  ctf_snapshot_id_t *id = (ctf_snapshot_id_t *) arg;

  return (atom->csa_snapshot_id > id->snapshot_id);
}

/* Roll back, deleting all atoms created after a particular ID.  */
void
ctf_str_rollback (ctf_file_t *fp, ctf_snapshot_id_t id)
{
  ctf_dynhash_iter_remove (fp->ctf_str_atoms, ctf_str_rollback_atom, &id);
}

/* An adaptor around ctf_purge_atom_refs.  */
static void
ctf_str_purge_one_atom_refs (void *key _libctf_unused_, void *value,
			     void *arg _libctf_unused_)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;
  ctf_str_purge_atom_refs (atom);
}

/* Remove all the recorded refs from the atoms table.  */
void
ctf_str_purge_refs (ctf_file_t *fp)
{
  if (fp->ctf_str_num_refs > 0)
    ctf_dynhash_iter (fp->ctf_str_atoms, ctf_str_purge_one_atom_refs, NULL);
  fp->ctf_str_num_refs = 0;
}

/* Update a list of refs to the specified value. */
static void
ctf_str_update_refs (ctf_str_atom_t *refs, uint32_t value)
{
  ctf_str_atom_ref_t *ref;

  for (ref = ctf_list_next (&refs->csa_refs); ref != NULL;
       ref = ctf_list_next (ref))
      *(ref->caf_ref) = value;
}

/* State shared across the strtab write process.  */
typedef struct ctf_strtab_write_state
{
  /* Strtab we are writing, and the number of strings in it.  */
  ctf_strs_writable_t *strtab;
  size_t strtab_count;

  /* Pointers to (existing) atoms in the atoms table, for qsorting.  */
  ctf_str_atom_t **sorttab;

  /* Loop counter for sorttab population.  */
  size_t i;

  /* The null-string atom (skipped during population).  */
  ctf_str_atom_t *nullstr;
} ctf_strtab_write_state_t;

/* Count the number of entries in the strtab, and its length.  */
static void
ctf_str_count_strtab (void *key _libctf_unused_, void *value,
	      void *arg)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;
  ctf_strtab_write_state_t *s = (ctf_strtab_write_state_t *) arg;

  /* We only factor in the length of items that have no offset:
     other items are in the external strtab.  They still contribute to the
     total count, though, because we still have to sort them.  */
  if (!atom->csa_offset)
    s->strtab->cts_len += strlen (atom->csa_str) + 1;
  s->strtab_count++;
}

/* Populate the sorttab with pointers to the strtab atoms.  */
static void
ctf_str_populate_sorttab (void *key _libctf_unused_, void *value,
		  void *arg)
{
  ctf_str_atom_t *atom = (ctf_str_atom_t *) value;
  ctf_strtab_write_state_t *s = (ctf_strtab_write_state_t *) arg;

  /* Skip the null string.  */
  if (s->nullstr == atom)
    return;

  s->sorttab[s->i++] = atom;
}

/* Sort the strtab.  */
static int
ctf_str_sort_strtab (const void *a, const void *b)
{
  ctf_str_atom_t **one = (ctf_str_atom_t **) a;
  ctf_str_atom_t **two = (ctf_str_atom_t **) b;

  return (strcmp ((*one)->csa_str, (*two)->csa_str));
}

/* Write out and return a strtab containing all strings with recorded refs,
   adjusting the refs to refer to the corresponding string.  The returned
   strtab may be NULL on error.  */
ctf_strs_writable_t
ctf_str_write_strtab (ctf_file_t *fp)
{
  ctf_strs_writable_t strtab;
  ctf_str_atom_t *nullstr;
  uint32_t cur_stroff = 0;
  ctf_strtab_write_state_t s;
  ctf_str_atom_t **sorttab;
  size_t i;

  memset (&strtab, 0, sizeof (struct ctf_strs_writable));
  memset (&s, 0, sizeof (struct ctf_strtab_write_state));
  s.strtab = &strtab;

  nullstr = ctf_dynhash_lookup (fp->ctf_str_atoms, "");
  if (!nullstr)
    {
      ctf_dprintf ("Internal error: null string not found in strtab.\n");
      strtab.cts_strs = NULL;
      return strtab;
    }

  ctf_dynhash_iter (fp->ctf_str_atoms, ctf_str_count_strtab, &s);

  ctf_dprintf ("%lu bytes of strings in strtab.\n",
	       (unsigned long) strtab.cts_len);

  /* Sort the strtab.  Force the null string to be first.  */
  sorttab = calloc (s.strtab_count, sizeof (ctf_str_atom_t *));
  if (!sorttab)
      return strtab;

  sorttab[0] = nullstr;
  s.i = 1;
  s.sorttab = sorttab;
  s.nullstr = nullstr;
  ctf_dynhash_iter (fp->ctf_str_atoms, ctf_str_populate_sorttab, &s);

  qsort (&sorttab[1], s.strtab_count - 1, sizeof (ctf_str_atom_t *),
	 ctf_str_sort_strtab);

  if ((strtab.cts_strs = ctf_alloc (strtab.cts_len)) == NULL)
    {
      free (sorttab);
      return strtab;
    }

  /* Update all refs: also update the strtab if this is not an external strtab
     pointer.  */
  for (i = 0; i < s.strtab_count; i++)
    {
      if (sorttab[i]->csa_offset)
	ctf_str_update_refs (sorttab[i], sorttab[i]->csa_offset);
      else
	{
	  ctf_str_update_refs (sorttab[i], cur_stroff);
	  strcpy (&strtab.cts_strs[cur_stroff], sorttab[i]->csa_str);
	  cur_stroff += strlen (sorttab[i]->csa_str) + 1;
	}
    }
  free (sorttab);

  return strtab;
}