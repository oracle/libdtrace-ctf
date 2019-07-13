/* Interface to hashtable implementations.
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.

   Not synced with GNU.  */

#include <ctf-impl.h>
#include <string.h>
#include <glib.h>

/* We have two hashtable implementations: one, ctf_dynhash_*(), is an interface to
   a dynamically-expanding hash with unknown size that should support addition
   of large numbers of items, and removal as well, and is used only at
   type-insertion time; the other, ctf_dynhash_*(), is an interface to a
   fixed-size hash from const char * -> ctf_id_t with number of elements
   specified at creation time, that should support addition of items but need
   not support removal.  These can be implemented by the same underlying hashmap
   if you wish.  */

/* ctf_dynhash is a purely-opaque type: it has no definition inside this file,
   where it is instead universally cast into the type of the *real* hash
   implementation.  */

static const uint32_t _CTF_EMPTY[1] = { 0 };

/* Hash functions. */

unsigned int
ctf_hash_integer (const void *ptr)
{
  /* Avoid g_direct_hash() for now: see glib commit dc983d74cc608f.  */

  return GPOINTER_TO_UINT (ptr) * 11;
}

int
ctf_hash_eq_integer (const void *a, const void *b)
{
  return g_direct_equal (a, b);
}

unsigned int
ctf_hash_string (const void *ptr)
{
  return g_str_hash (ptr);
}

int
ctf_hash_eq_string (const void *a, const void *b)
{
  return g_str_equal (a, b);
}

/* Hash a type_mapping_key.  */
unsigned int
ctf_hash_type_mapping_key (const void *ptr)
{
  ctf_link_type_mapping_key_t *k = (ctf_link_type_mapping_key_t *) ptr;
  return GPOINTER_TO_UINT (k->cltm_fp) * 11
    + 59 * GPOINTER_TO_UINT (k->cltm_idx) * 13;
}

int
ctf_hash_eq_type_mapping_key (const void *a, const void *b)
{
  ctf_link_type_mapping_key_t *key_a = (ctf_link_type_mapping_key_t *) a;
  ctf_link_type_mapping_key_t *key_b = (ctf_link_type_mapping_key_t *) b;

  return (key_a->cltm_fp == key_b->cltm_fp)
    && (key_a->cltm_idx == key_b->cltm_idx);
}

/* The dynhash, used for hashes whose size is not known at creation time.
   Implemented using GHashTable, an expanding hash.  */

ctf_dynhash_t *
ctf_dynhash_create (ctf_hash_fun hash_fun, ctf_hash_eq_fun eq_fun,
		    ctf_hash_free_fun key_free,
		    ctf_hash_free_fun value_free)
{
  return (ctf_dynhash_t *) g_hash_table_new_full ((GHashFunc) hash_fun,
						  (GEqualFunc) eq_fun,
						  (GDestroyNotify) key_free,
						  (GDestroyNotify) value_free);
}

int
ctf_dynhash_insert (ctf_dynhash_t *hp, void *key, void *value)
{
  g_hash_table_insert ((GHashTable *) hp, (gpointer) key, value);
  return 0;
}

void
ctf_dynhash_remove (ctf_dynhash_t *hp, const void *key)
{
  g_hash_table_remove ((GHashTable *) hp, key);
}

void *
ctf_dynhash_lookup (ctf_dynhash_t *hp, const void *key)
{
  return g_hash_table_lookup ((GHashTable *) hp, key);
}

void
ctf_dynhash_iter (ctf_dynhash_t *hp, ctf_hash_iter_f fun, void *arg)
{
  g_hash_table_foreach ((GHashTable *)hp, fun, arg);
}

void
ctf_dynhash_iter_remove (ctf_dynhash_t *hp, ctf_hash_iter_remove_f fun,
			 void *arg)
{
  g_hash_table_foreach_remove ((GHashTable *)hp, fun, arg);
}

void
ctf_dynhash_destroy (ctf_dynhash_t *hp)
{
  if (hp != NULL)
    g_hash_table_destroy ((GHashTable *) hp);
}

/* ctf_hash, used for fixed-size maps from const char * -> ctf_id_t without
   removal.  TODO: implement one-shot initialization with precomputed bucket
   distribution.  */

typedef struct ctf_helem
{
  uint32_t h_name;		/* Reference to name in string table.  */
  uint32_t h_next;		/* Index of next element in hash chain.  */
  ctf_id_t h_type;		/* Corresponding type ID number.  */
} ctf_helem_t;

typedef struct ctf_fixed_hash
{
  ctf_helem_t *h_chains;	/* Hash chains buffer.  */
  uint32_t *h_buckets;		/* Hash bucket array (chain indices).  */
  uint32_t h_nbuckets;		/* Number of elements in bucket array.  */
  uint32_t h_nelems;		/* Number of elements in hash table.  */
  uint32_t h_free;		/* Index of next free hash element.  */
} ctf_hash_t;

/* Table of primes up to almost as high as it's worth going -- there is no point
   going all the way up to 2^32: an average chain length of four is not a
   disaster.  */

static uint32_t const primes[] = { 1021, 2039, 4093, 8191, 16381,
				   32749, 65521, 131071, 262139,
				   524287, 1048573, 2097143,
				   4194301, 8388593, 16777213,
				   33554393, 67108859, 134217689,
				   268435399, 536870909, 1073741789 };

/* Find a prime greater than N, near a power of two.  */

static uint32_t
find_prime (unsigned long n)
{
  uint32_t low = 0;
  uint32_t high = sizeof(primes) / sizeof(uint32_t);

  while (low != high)
    {
      uint32_t mid = low + (high - low) / 2;
      if (n > primes[mid])
	low = mid + 1;
      else
	high = mid;
    }
  return primes[low];
}

ctf_hash_t *
ctf_hash_create (unsigned long nelems, ctf_hash_fun hash_fun,
		 ctf_hash_eq_fun eq_fun)
{
  ctf_hash_t *hp;

  if ((hash_fun != ctf_hash_string) || (eq_fun != ctf_hash_eq_string))
    {
      errno = EINVAL;
      return NULL;
    }

  if (nelems > UINT32_MAX)
    {
      errno = EOVERFLOW;
      return NULL;
    }

  if ((hp = malloc (sizeof (ctf_hash_t))) == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }

  /* If the hash table is going to be empty, don't bother allocating any
     memory and make the only bucket point to a zero so lookups fail.  */

  if (nelems == 0)
    {
      memset (hp, 0, sizeof (ctf_hash_t));
      hp->h_buckets = (uint32_t *) _CTF_EMPTY;
      hp->h_nbuckets = 1;
      return hp;
    }

  hp->h_nbuckets = find_prime (nelems);
  hp->h_nelems = nelems + 1;	/* We use index zero as a sentinel.  */
  hp->h_free = 1;		/* First free element is index 1.  */

  hp->h_buckets = calloc (hp->h_nbuckets, sizeof (uint32_t));
  hp->h_chains = calloc (hp->h_nelems, sizeof (ctf_helem_t));

  if (hp->h_buckets == NULL || hp->h_chains == NULL)
    {
      ctf_hash_destroy (hp);
      errno = ENOMEM;
      return NULL;
    }

  return hp;
}

uint32_t
ctf_hash_size (const ctf_hash_t *hp)
{
  return (hp->h_nelems ? hp->h_nelems - 1 : 0);
}

int
ctf_hash_insert_type (ctf_hash_t * hp, ctf_file_t * fp, uint32_t type,
		      uint32_t name)
{
  ctf_strs_t *ctsp = &fp->ctf_str[CTF_NAME_STID (name)];
  const char *str = ctsp->cts_strs + CTF_NAME_OFFSET (name);
  ctf_helem_t *hep = &hp->h_chains[hp->h_free];
  uint32_t h;

  if (type == 0)
    return EINVAL;

  if (hp->h_free >= hp->h_nelems)
    return EOVERFLOW;

  if (ctsp->cts_strs == NULL)
    return ECTF_STRTAB;

  if (ctsp->cts_len <= CTF_NAME_OFFSET (name))
    return ECTF_BADNAME;

  if (str[0] == '\0')
    return 0;		   /* Just ignore empty strings on behalf of caller.  */

  hep->h_name = name;
  hep->h_type = type;
  h = ctf_hash_string (str) % hp->h_nbuckets;
  hep->h_next = hp->h_buckets[h];
  hp->h_buckets[h] = hp->h_free++;

  return 0;
}

/* Wrapper for ctf_hash_lookup_type/ctf_hash_insert_type: if the key is already
   in the hash, override the previous definition with this new official
   definition.  If the key is not present, then call ctf_hash_insert_type() and
   hash it in.  */
int
ctf_hash_define_type (ctf_hash_t *hp, ctf_file_t *fp, uint32_t type,
		 uint32_t name)
{
  const char *str = ctf_strptr (fp, name);
  ctf_id_t id = ctf_hash_lookup_type (hp, fp, str);

  if (id == 0)
    return (ctf_hash_insert_type (hp, fp, type, name));

  return 0;
}

ctf_id_t
ctf_hash_lookup_type (ctf_hash_t *hp, ctf_file_t *fp, const char *key)
{
  ctf_helem_t *hep;
  ctf_strs_t *ctsp;
  const char *str;
  size_t i;
  size_t j = 0;

  uint32_t h = ctf_hash_string (key) % hp->h_nbuckets;

  for (i = hp->h_buckets[h]; i != 0; i = hep->h_next, j++)
    {
      hep = &hp->h_chains[i];
      ctsp = &fp->ctf_str[CTF_NAME_STID (hep->h_name)];
      str = ctsp->cts_strs + CTF_NAME_OFFSET (hep->h_name);
      if (strcmp (key, str) == 0)
	return hep->h_type;
    }

  return 0; 		/* Sentinel value.  */
}

void
ctf_hash_destroy (ctf_hash_t *hp)
{
  if (hp == NULL)
    return;

  if (hp->h_buckets != NULL && hp->h_nbuckets != 1)
    {
      free (hp->h_buckets);
      hp->h_buckets = NULL;
    }

  if (hp->h_chains != NULL)
    {
      free (hp->h_chains);
      hp->h_chains = NULL;
    }
  free (hp);
}
