/* Miscellaneous utilities.
   Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#include <ctf-impl.h>
#include <string.h>

/* Simple doubly-linked list append routine.  This implementation assumes that
   each list element contains an embedded ctf_list_t as the first member.
   An additional ctf_list_t is used to store the head (l_next) and tail
   (l_prev) pointers.  The current head and tail list elements have their
   previous and next pointers set to NULL, respectively.  */

void
ctf_list_append (ctf_list_t *lp, void *newp)
{
  ctf_list_t *p = lp->l_prev;	/* p = tail list element.  */
  ctf_list_t *q = newp;		/* q = new list element.  */

  lp->l_prev = q;
  q->l_prev = p;
  q->l_next = NULL;

  if (p != NULL)
    p->l_next = q;
  else
    lp->l_next = q;
}

/* Prepend the specified existing element to the given ctf_list_t.  The
   existing pointer should be pointing at a struct with embedded ctf_list_t.  */

void
ctf_list_prepend (ctf_list_t * lp, void *newp)
{
  ctf_list_t *p = newp;		/* p = new list element.  */
  ctf_list_t *q = lp->l_next;	/* q = head list element.  */

  lp->l_next = p;
  p->l_prev = NULL;
  p->l_next = q;

  if (q != NULL)
    q->l_prev = p;
  else
    lp->l_prev = p;
}

/* Delete the specified existing element from the given ctf_list_t.  The
   existing pointer should be pointing at a struct with embedded ctf_list_t.  */

void
ctf_list_delete (ctf_list_t *lp, void *existing)
{
  ctf_list_t *p = existing;

  if (p->l_prev != NULL)
    p->l_prev->l_next = p->l_next;
  else
    lp->l_next = p->l_next;

  if (p->l_next != NULL)
    p->l_next->l_prev = p->l_prev;
  else
    lp->l_prev = p->l_prev;
}

/* Convert a 32-bit ELF symbol into Elf64 and return a pointer to it.  */

Elf64_Sym *
ctf_sym_to_elf64 (const Elf32_Sym *src, Elf64_Sym *dst)
{
  dst->st_name = src->st_name;
  dst->st_value = src->st_value;
  dst->st_size = src->st_size;
  dst->st_info = src->st_info;
  dst->st_other = src->st_other;
  dst->st_shndx = src->st_shndx;

  return dst;
}

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

const char *
ctf_strptr (ctf_file_t *fp, uint32_t name)
{
  const char *s = ctf_strraw (fp, name);
  return (s != NULL ? s : "(?)");
}

/* Same as strdup(3C), but use ctf_alloc() to do the memory allocation. */

_libctf_malloc_ char *
ctf_strdup (const char *s1)
{
  char *s2 = ctf_alloc (strlen (s1) + 1);

  if (s2 != NULL)
    (void) strcpy (s2, s1);

  return s2;
}

/* A string appender working on dynamic strings.  */

char *
ctf_str_append (char *s, const char *append)
{
  size_t s_len = 0;

  if (append == NULL)
    return s;

  if (s != NULL)
    s_len = strlen (s);

  size_t append_len = strlen (append);

  if ((s = realloc (s, s_len + append_len + 1)) == NULL)
    return NULL;

  memcpy (s + s_len, append, append_len);
  s[s_len + append_len] = '\0';

  return s;
}

/* Store the specified error code into errp if it is non-NULL, and then
   return NULL for the benefit of the caller.  */

void *
ctf_set_open_errno (int *errp, int error)
{
  if (errp != NULL)
    *errp = error;
  return NULL;
}

/* Store the specified error code into the CTF container, and then return
   CTF_ERR for the benefit of the caller. */

long
ctf_set_errno (ctf_file_t * fp, int err)
{
  fp->ctf_errno = err;
  return CTF_ERR;
}
