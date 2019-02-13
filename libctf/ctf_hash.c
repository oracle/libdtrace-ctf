/* CTF hash functions.

   Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <string.h>
#include <ctf_impl.h>

static const unsigned short _CTF_EMPTY[1] = { 0 };

/* TODO: switch to a proper expanding hash table implementation.  */

/* XXX before GNUizing: add an abstraction layer so that libiberty hashtab can
   supplant this.  */

int
ctf_hash_create (ctf_hash_t *hp, unsigned long nelems)
{
  if (nelems > UINT32_MAX)
    return EOVERFLOW;

  /* If the hash table is going to be empty, don't bother allocating any
     memory and make the only bucket point to a zero so lookups fail.  */

  if (nelems == 0)
    {
      memset (hp, 0, sizeof (ctf_hash_t));
      hp->h_buckets = (unsigned short *) _CTF_EMPTY;
      hp->h_nbuckets = 1;
      return 0;
    }

  hp->h_nbuckets = 8191;	/* Use a prime number of hash buckets.  */
  hp->h_nelems = nelems + 1;	/* We use index zero as a sentinel.  */
  hp->h_free = 1;		/* First free element is index 1.  */

  hp->h_buckets = ctf_alloc (sizeof (unsigned short) * hp->h_nbuckets);
  hp->h_chains = ctf_alloc (sizeof (ctf_helem_t) * hp->h_nelems);

  if (hp->h_buckets == NULL || hp->h_chains == NULL)
    {
      ctf_hash_destroy (hp);
      return EAGAIN;
    }

  memset (hp->h_buckets, 0, sizeof (unsigned short) * hp->h_nbuckets);
  memset (hp->h_chains, 0, sizeof (ctf_helem_t) * hp->h_nelems);

  return 0;
}

uint32_t
ctf_hash_size (const ctf_hash_t *hp)
{
  return (hp->h_nelems ? hp->h_nelems - 1 : 0);
}

unsigned long
ctf_hash_compute (const char *key, size_t len)
{
  unsigned long g, h = 0;
  const char *p, *q = key + len;
  size_t n = 0;

  for (p = key; p < q; p++, n++)
    {
      h = (h << 4) + *p;

      if ((g = (h & 0xf0000000)) != 0)
	{
	  h ^= (g >> 24);
	  h ^= g;
	}
    }

  return h;
}

int
ctf_hash_insert (ctf_hash_t * hp, ctf_file_t * fp, uint32_t type,
		 uint32_t name)
{
  ctf_strs_t *ctsp = &fp->ctf_str[CTF_NAME_STID (name)];
  const char *str = ctsp->cts_strs + CTF_NAME_OFFSET (name);
  ctf_helem_t *hep = &hp->h_chains[hp->h_free];
  unsigned long h;

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
  h = ctf_hash_compute (str, strlen (str)) % hp->h_nbuckets;
  hep->h_next = hp->h_buckets[h];
  hp->h_buckets[h] = hp->h_free++;

  return 0;
}

/* Wrapper for ctf_hash_lookup/ctf_hash_insert: if the key is already in the
   hash, override the previous definition with this new official definition.
   If the key is not present, then call ctf_hash_insert() and hash it in.  */
int
ctf_hash_define (ctf_hash_t *hp, ctf_file_t *fp, uint32_t type,
		 uint32_t name)
{
  const char *str = ctf_strptr (fp, name);
  ctf_helem_t *hep = ctf_hash_lookup (hp, fp, str, strlen (str));

  if (hep == NULL)
    return (ctf_hash_insert (hp, fp, type, name));

  hep->h_type = type;
  return 0;
}

ctf_helem_t *
ctf_hash_lookup (ctf_hash_t *hp, ctf_file_t *fp, const char *key,
		 size_t len)
{
  ctf_helem_t *hep;
  ctf_strs_t *ctsp;
  const char *str;
  unsigned short i;

  unsigned long h = ctf_hash_compute (key, len) % hp->h_nbuckets;

  for (i = hp->h_buckets[h]; i != 0; i = hep->h_next)
    {
      hep = &hp->h_chains[i];
      ctsp = &fp->ctf_str[CTF_NAME_STID (hep->h_name)];
      str = ctsp->cts_strs + CTF_NAME_OFFSET (hep->h_name);

      if (strncmp (key, str, len) == 0 && str[len] == '\0')
	return hep;
    }

  return NULL;
}

void
ctf_hash_destroy (ctf_hash_t *hp)
{
  if (hp->h_buckets != NULL && hp->h_nbuckets != 1)
    {
      ctf_free (hp->h_buckets, sizeof (unsigned short) * hp->h_nbuckets);
      hp->h_buckets = NULL;
    }

  if (hp->h_chains != NULL)
    {
      ctf_free (hp->h_chains, sizeof (ctf_helem_t) * hp->h_nelems);
      hp->h_chains = NULL;
    }
}
