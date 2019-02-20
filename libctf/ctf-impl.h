/* Implementation header.
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

   Licensed under the Universal Permissive License v 1.0 as shown at
   http://oss.oracle.com/licenses/upl.

   Licensed under the GNU General Public License (GPL), version 2. See the file
   COPYING in the top level of this tree.  */

#ifndef	_CTF_IMPL_H
#define	_CTF_IMPL_H

#include <sys/errno.h>
#include <sys/ctf-api.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>

#ifdef	__cplusplus
extern "C"
  {
#endif

/* Compiler attributes.  */

#if defined (__GNUC__)

/* GCC.  We assume that all compilers claiming to be GCC support sufficiently
   many GCC attributes that the code below works.  If some non-GCC compilers
   masquerading as GCC in fact do not implement these attributes, version checks
   may be required.  */

/* We use the _libctf_*_ pattern to avoid clashes with any future attribute
   macros glibc may introduce, which have names of the pattern
   __attribute_blah__.  */

#define _libctf_constructor_(x) __attribute__ ((__constructor__))
#define _libctf_destructor_(x) __attribute__ ((__destructor__))
#define _libctf_printflike_(string_index,first_to_check) \
    __attribute__ ((__format__ (__printf__, (string_index), (first_to_check))))
#define _libctf_unlikely_(x) __builtin_expect ((x), 0)
#define _libctf_unused_ __attribute__ ((__unused__))

#endif

/* libctf in-memory state.  */

typedef struct ctf_helem
{
  uint32_t h_name;		/* Reference to name in string table.  */
  uint32_t h_type;		/* Corresponding type ID number.  */
  uint32_t h_next;		/* Index of next element in hash chain.  */
} ctf_helem_t;

typedef struct ctf_hash
{
  unsigned short *h_buckets;	/* Hash bucket array (chain indices).  */
  ctf_helem_t *h_chains;	/* Hash chains buffer.  */
  unsigned short h_nbuckets;	/* Number of elements in bucket array.  */
  uint32_t h_nelems;		/* Number of elements in hash table.  */
  uint32_t h_free;		/* Index of next free hash element.  */
} ctf_hash_t;

typedef struct ctf_strs
{
  const char *cts_strs;		/* Base address of string table.  */
  size_t cts_len;		/* Size of string table in bytes.  */
} ctf_strs_t;

typedef struct ctf_dmodel
{
  const char *ctd_name;		/* Data model name.  */
  int ctd_code;			/* Data model code.  */
  size_t ctd_pointer;		/* Size of void * in bytes.  */
  size_t ctd_char;		/* Size of char in bytes.  */
  size_t ctd_short;		/* Size of short in bytes.  */
  size_t ctd_int;		/* Size of int in bytes.  */
  size_t ctd_long;		/* Size of long in bytes.  */
} ctf_dmodel_t;

typedef struct ctf_lookup
{
  const char *ctl_prefix;	/* String prefix for this lookup.  */
  size_t ctl_len;		/* Length of prefix string in bytes.  */
  ctf_hash_t *ctl_hash;		/* Pointer to hash table for lookup.  */
} ctf_lookup_t;

typedef struct ctf_fileops
{
  uint32_t (*ctfo_get_kind) (uint32_t);
  uint32_t (*ctfo_get_root) (uint32_t);
  uint32_t (*ctfo_get_vlen) (uint32_t);
  ssize_t (*ctfo_get_ctt_size) (const ctf_file_t *, const ctf_type_t *,
				ssize_t *, ssize_t *);
  ssize_t (*ctfo_get_vbytes) (unsigned short, ssize_t, size_t);
} ctf_fileops_t;

typedef struct ctf_list
{
  struct ctf_list *l_prev;	/* Previous pointer or tail pointer.  */
  struct ctf_list *l_next;	/* Next pointer or head pointer.  */
} ctf_list_t;

typedef enum
  {
   CTF_PREC_BASE,
   CTF_PREC_POINTER,
   CTF_PREC_ARRAY,
   CTF_PREC_FUNCTION,
   CTF_PREC_MAX
  } ctf_decl_prec_t;

typedef struct ctf_decl_node
{
  ctf_list_t cd_list;		/* Linked list pointers.  */
  ctf_id_t cd_type;		/* Type identifier.  */
  uint32_t cd_kind;		/* Type kind.  */
  uint32_t cd_n;		/* Type dimension if array.  */
} ctf_decl_node_t;

typedef struct ctf_decl
{
  ctf_list_t cd_nodes[CTF_PREC_MAX]; /* Declaration node stacks.  */
  int cd_order[CTF_PREC_MAX];	     /* Storage order of decls.  */
  ctf_decl_prec_t cd_qualp;	     /* Qualifier precision.  */
  ctf_decl_prec_t cd_ordp;	     /* Ordered precision.  */
  char *cd_buf;			     /* Buffer for output.  */
  char *cd_ptr;			     /* Buffer location.  */
  char *cd_end;			     /* Buffer limit.  */
  size_t cd_len;		     /* Buffer space required.  */
  int cd_err;			     /* Saved error value.  */
} ctf_decl_t;

typedef struct ctf_dmdef
{
  ctf_list_t dmd_list;		/* List forward/back pointers.  */
  char *dmd_name;		/* Name of this member.  */
  ctf_id_t dmd_type;		/* Type of this member (for sou).  */
  unsigned long dmd_offset;	/* Offset of this member in bits (for sou).  */
  int dmd_value;		/* Value of this member (for enum).  */
} ctf_dmdef_t;

typedef struct ctf_dtdef
{
  ctf_list_t dtd_list;		/* List forward/back pointers.  */
  struct ctf_dtdef *dtd_hash;	/* Hash chain pointer for ctf_dthash.  */
  char *dtd_name;		/* Name associated with definition (if any).  */
  ctf_id_t dtd_type;		/* Type identifier for this definition.  */
  ctf_type_t dtd_data;		/* Type node (see <sys/ctf.h>).  */
  union
  {
    ctf_list_t dtu_members;	/* struct, union, or enum */
    ctf_arinfo_t dtu_arr;	/* array */
    ctf_encoding_t dtu_enc;	/* integer or float */
    ctf_id_t *dtu_argv;		/* function */
  } dtd_u;
} ctf_dtdef_t;

typedef struct ctf_dvdef
{
  ctf_list_t dvd_list;		/* List forward/back pointers.  */
  struct ctf_dvdef *dvd_hash;	/* Hash chain pointer for ctf_dvhash.  */
  char *dvd_name;		/* Name associated with variable.  */
  ctf_id_t dvd_type;		/* Type of variable.  */
  unsigned long dvd_snapshots;	/* Snapshot count when inserted.  */
} ctf_dvdef_t;

typedef struct ctf_bundle
{
  ctf_file_t *ctb_file;		/* CTF container handle.  */
  ctf_id_t ctb_type;		/* CTF type identifier.  */
  ctf_dtdef_t *ctb_dtd;		/* CTF dynamic type definition (if any).  */
} ctf_bundle_t;

/* The ctf_file is the structure used to represent a CTF container to library
   clients, who see it only as an opaque pointer.  Modifications can therefore
   be made freely to this structure without regard to client versioning.  The
   ctf_file_t typedef appears in <sys/ctf-api.h> and declares a forward tag.

   NOTE: ctf_update() requires that everything inside of ctf_file either be an
   immediate value, a pointer to dynamically allocated data *outside* of the
   ctf_file itself, or a pointer to statically allocated data.  If you add a
   pointer to ctf_file that points to something within the ctf_file itself,
   you must make corresponding changes to ctf_update().  */

struct ctf_file
{
  const ctf_fileops_t *ctf_fileops; /* Version-specific file operations.  */
  ctf_sect_t ctf_data;		    /* CTF data from object file.  */
  ctf_sect_t ctf_symtab;	    /* Symbol table from object file.  */
  ctf_sect_t ctf_strtab;	    /* String table from object file.  */
  ctf_hash_t ctf_structs;	    /* Hash table of struct types.  */
  ctf_hash_t ctf_unions;	    /* Hash table of union types.  */
  ctf_hash_t ctf_enums;		    /* Hash table of enum types.  */
  ctf_hash_t ctf_names;		    /* Hash table of remaining type names.  */
  ctf_lookup_t ctf_lookups[5];	    /* Pointers to hashes for name lookup.  */
  ctf_strs_t ctf_str[2];	    /* Array of string table base and bounds.  */
  const unsigned char *ctf_base;  /* Base of CTF header + uncompressed buffer.  */
  const unsigned char *ctf_buf;	  /* Uncompressed CTF data buffer.  */
  size_t ctf_size;		  /* Size of CTF header + uncompressed data.  */
  uint32_t *ctf_sxlate;		  /* Translation table for symtab entries.  */
  unsigned long ctf_nsyms;	  /* Number of entries in symtab xlate table.  */
  uint32_t *ctf_txlate;		  /* Translation table for type IDs.  */
  uint32_t *ctf_ptrtab;		  /* Translation table for pointer-to lookups.  */
  struct ctf_varent *ctf_vars;	  /* Sorted variable->type mapping.  */
  unsigned long ctf_nvars;	  /* Number of variables in ctf_vars.  */
  unsigned long ctf_typemax;	  /* Maximum valid type ID number.  */
  const ctf_dmodel_t *ctf_dmodel; /* Data model pointer (see above).  */
  struct ctf_file *ctf_parent;	  /* Parent CTF container (if any).  */
  const char *ctf_parlabel;	  /* Label in parent container (if any).  */
  const char *ctf_parname;	  /* Basename of parent (if any).  */
  char *ctf_dynparname;		  /* Dynamically allocated name of parent.  */
  uint32_t ctf_parmax;		  /* Highest type ID of a parent type.  */
  uint32_t ctf_refcnt;		  /* Reference count (for parent links).  */
  uint32_t ctf_flags;		  /* Libctf flags (see below).  */
  int ctf_errno;		  /* Error code for most recent error.  */
  int ctf_version;		  /* CTF data version.  */
  ctf_dtdef_t **ctf_dthash;	  /* Hash of dynamic type definitions.  */
  unsigned long ctf_dthashlen;	  /* Size of dynamic type hash bucket array.  */
  ctf_list_t ctf_dtdefs;	  /* List of dynamic type definitions.  */
  ctf_dvdef_t **ctf_dvhash;	  /* Hash of dynamic variable mappings.  */
  unsigned long ctf_dvhashlen;	  /* Size of dynvar hash bucket array.  */
  ctf_list_t ctf_dvdefs;	  /* List of dynamic variable definitions.  */
  size_t ctf_dtvstrlen;		  /* Total length of dynamic type+var strings.  */
  unsigned long ctf_dtnextid;	  /* Next dynamic type id to assign.  */
  unsigned long ctf_dtoldid;	  /* Oldest id that has been committed.  */
  unsigned long ctf_snapshots;	  /* ctf_snapshot() plus ctf_update() count.  */
  unsigned long ctf_snapshot_lu;  /* ctf_snapshot() call count at last update.  */
  void *ctf_specific;		  /* Data for ctf_get/setspecific().  */
};

/* The ctf_archive is a collection of ctf_file_t's stored together. The format
   is suitable for mmap()ing: this control structure merely describes the
   mmap()ed archive (and overlaps the first few bytes of it), hence the
   greater care taken with integral types.  All CTF files in an archive
   must have the same data model.  (This is not validated.)

   All integers in this structure are stored in little-endian byte order.

   The code relies on the fact that everything in this header is a uint64_t
   and thus the header needs no padding (in particular, that no padding is
   needed between ctfa_ctfs and the unnamed ctfa_archive_modent array
   that follows it).  */

#define CTFA_MAGIC 0x8b47f2a4d7623eeb	/* Random.  */
struct ctf_archive
{
  /* Magic number.  (In loaded files, overwritten with the file size
     so ctf_arc_close() knows how much to munmap()).  */
  uint64_t ctfa_magic;

  /* CTF data model.  */
  uint64_t ctfa_model;

  /* Number of CTF files in the archive.  */
  uint64_t ctfa_nfiles;

  /* Offset of the name table.  */
  uint64_t ctfa_names;

  /* Offset of the CTF table.  Each element starts with a size (a uint64_t
     in network byte order) then a ctf_file_t of that size.  */
  uint64_t ctfa_ctfs;
};

/* An array of ctfa_nnamed of this structure lies at
   ctf_archive[ctf_archive->ctfa_modents] and gives the ctfa_ctfs or
   ctfa_names-relative offsets of each name or ctf_file_t.  */

typedef struct ctf_archive_modent
{
  uint64_t name_offset;
  uint64_t ctf_offset;
} ctf_archive_modent_t;

/* Return x rounded up to an alignment boundary.
   eg, P2ROUNDUP(0x1234, 0x100) == 0x1300 (0x13*align)
   eg, P2ROUNDUP(0x5600, 0x100) == 0x5600 (0x56*align)  */
#define P2ROUNDUP(x, align)		(-(-(x) & -(align)))

/* * If an offs is not aligned already then round it up and align it. */
#define LCTF_ALIGN_OFFS(offs, align) ((offs + (align - 1)) & ~(align - 1))

#define LCTF_TYPE_ISPARENT(fp, id) ((id) <= fp->ctf_parmax)
#define LCTF_TYPE_ISCHILD(fp, id) ((id) > fp->ctf_parmax)
#define LCTF_TYPE_TO_INDEX(fp, id) ((id) & (fp->ctf_parmax))
#define LCTF_INDEX_TO_TYPE(fp, id, child) (child ? ((id) | (fp->ctf_parmax+1)) : \
					   (id))

#define LCTF_INDEX_TO_TYPEPTR(fp, i) \
  ((ctf_type_t *)((uintptr_t)(fp)->ctf_buf + (fp)->ctf_txlate[(i)]))

#define LCTF_INFO_KIND(fp, info)	((fp)->ctf_fileops->ctfo_get_kind(info))
#define LCTF_INFO_ISROOT(fp, info)	((fp)->ctf_fileops->ctfo_get_root(info))
#define LCTF_INFO_VLEN(fp, info)	((fp)->ctf_fileops->ctfo_get_vlen(info))
#define LCTF_VBYTES(fp, kind, size, vlen) \
  ((fp)->ctf_fileops->ctfo_get_vbytes(kind, size, vlen))

static inline ssize_t ctf_get_ctt_size (const ctf_file_t* fp,
					const ctf_type_t* tp,
					ssize_t *sizep,
					ssize_t *incrementp)
{
  return (fp->ctf_fileops->ctfo_get_ctt_size (fp, tp, sizep, incrementp));
}

#define LCTF_MMAP	0x0001	/* libctf should munmap buffers on close.  */
#define LCTF_CHILD	0x0002	/* CTF container is a child */
#define LCTF_RDWR	0x0004	/* CTF container is writable */
#define LCTF_DIRTY	0x0008	/* CTF container has been modified */

extern const ctf_type_t *ctf_lookup_by_id (ctf_file_t **, ctf_id_t);

extern int ctf_hash_create (ctf_hash_t *, unsigned long);
extern int ctf_hash_insert (ctf_hash_t *, ctf_file_t *, uint32_t, uint32_t);
extern int ctf_hash_define (ctf_hash_t *, ctf_file_t *, uint32_t, uint32_t);
extern ctf_helem_t *ctf_hash_lookup (ctf_hash_t *, ctf_file_t *,
				     const char *, size_t);
extern uint32_t ctf_hash_size (const ctf_hash_t *);
extern unsigned long ctf_hash_compute (const char *key, size_t len);
extern void ctf_hash_destroy (ctf_hash_t *);

#define	ctf_list_prev(elem)	((void *)(((ctf_list_t *)(elem))->l_prev))
#define	ctf_list_next(elem)	((void *)(((ctf_list_t *)(elem))->l_next))

extern void ctf_list_append (ctf_list_t *, void *);
extern void ctf_list_prepend (ctf_list_t *, void *);
extern void ctf_list_delete (ctf_list_t *, void *);

extern void ctf_dtd_insert (ctf_file_t *, ctf_dtdef_t *);
extern void ctf_dtd_delete (ctf_file_t *, ctf_dtdef_t *);
extern ctf_dtdef_t *ctf_dtd_lookup (ctf_file_t *, ctf_id_t);

extern void ctf_dvd_insert (ctf_file_t *, ctf_dvdef_t *);
extern void ctf_dvd_delete (ctf_file_t *, ctf_dvdef_t *);
extern ctf_dvdef_t *ctf_dvd_lookup (ctf_file_t *, const char *);

extern void ctf_decl_init (ctf_decl_t *, char *, size_t);
extern void ctf_decl_fini (ctf_decl_t *);
extern void ctf_decl_push (ctf_decl_t *, ctf_file_t *, ctf_id_t);

_libctf_printflike_ (2, 3)
extern void ctf_decl_sprintf (ctf_decl_t *, const char *, ...);

extern const char *ctf_strraw (ctf_file_t *, uint32_t);
extern const char *ctf_strptr (ctf_file_t *, uint32_t);

extern ctf_file_t *ctf_set_open_errno (int *, int);
extern long ctf_set_errno (ctf_file_t *, int);

extern const void *ctf_sect_mmap (ctf_sect_t *, int);
extern void ctf_sect_munmap (const ctf_sect_t *);

extern void *ctf_data_alloc (size_t);
extern void ctf_data_free (void *, size_t);
extern void ctf_data_protect (void *, size_t);

extern void *ctf_alloc (size_t);
extern void ctf_free (void *, size_t);

extern char *ctf_strdup (const char *);
extern const char *ctf_strerror (int);

_libctf_printflike_ (1, 2)
extern void ctf_dprintf (const char *, ...);

/* Variables, all underscore-prepended. */

extern const char _CTF_SECTION[];	/* name of CTF ELF section */
extern const char _CTF_NULLSTR[];	/* empty string */

extern int _libctf_version;	/* library client version */
extern int _libctf_debug;	/* debugging messages enabled */

#ifdef	__cplusplus
}
#endif

#endif /* _CTF_IMPL_H */
