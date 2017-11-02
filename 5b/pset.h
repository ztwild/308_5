#ifndef _SCHED_PSET_H
#define _SCHED_PSET_H

/* processor set prototype header file */
/* 2001-02-15   deleted unnecessary PSET_GETDFLTPSET */

typedef int psetid_t;
typedef int id_t;

#define PS_DEFAULT      ((psetid_t) 0)
#define PS_NONE         PS_DEFAULT /* synonym */
#define PS_MYID		-1
#define PS_QUERY	-1
#define PS_MAXPSET      0x8000     /* should never need over one set per pid */

typedef enum       /* matches /usr/include/sys/wait.h */
{
  P_ALL,
  P_PID,
  P_PGID
} idtype_t;

typedef enum pset_request {
	PSET_GETTICKS		= 0,
        PSET_GETNUMPSETS        = 1,
        PSET_GETFIRSTPSET       = 2,
        PSET_GETNEXTPSET        = 3,
        PSET_GETCURRENTPSET     = 4,
        PSET_GETNUMSPUS         = 5,
        PSET_GETFIRSTSPU        = 6,
        PSET_GETNEXTSPU         = 7,
        PSET_SPUTOPSET          = 8
} pset_request_t ;

typedef enum pset_attrval {
        PSET_ATTRVAL_DEFAULT    = 0, /* unimplemented */
        PSET_ATTRVAL_ALLOW      = 1, /* unimplemented */
        PSET_ATTRVAL_DISALLOW   = 2, /* unimplemented */
        PSET_ATTRVAL_FAIL       = 3, /* unimplemented */
        PSET_ATTRVAL_FAILBUSY   = 4,
        PSET_ATTRVAL_DFLTPSET   = 5,
        PSET_ATTRVAL_KILL       = 6,
        PSET_ATTRVAL_STOP       = 7  /* unimplemented */
} pset_attrval_t ;

typedef enum pset_attrtype {
        PSET_ATTR_OWNID         = 1, /* unimplemented */
        PSET_ATTR_GRPID         = 2, /* unimplemented */
        PSET_ATTR_PERM          = 3, /* unimplemented */
        PSET_ATTR_IOINTR        = 4, /* unimplemented */
        PSET_ATTR_NONEMPTY      = 5,
        PSET_ATTR_EMPTY         = 6, /* unimplemented */
        PSET_ATTR_LASTSPU       = 7  /* unimplemented */
} pset_attrtype_t ;

#define PSIOC_BASE      100
#define PSIOC_CREATE	1
#define PSIOC_DESTROY	2
#define PSIOC_ASSIGN	3
#define PSIOC_BIND	4
#define PSIOC_GETATTR	5
#define PSIOC_SETATTR   6
#define PSIOC_CTL	7

typedef psetid_t * pset_create_t;
typedef psetid_t   pset_destroy_t;

typedef struct pset_assign_args {
        psetid_t pset;
        int      spu;
        psetid_t *opset;
} pset_assign_t;

typedef struct pset_bind_args {
        psetid_t pset;
        idtype_t idtype;
        id_t     id;
        psetid_t *opset;
} pset_bind_t;

typedef struct pset_getattr_args {
        psetid_t        pset;
        pset_attrtype_t type;
        pset_attrval_t  *value;
} pset_getattr_t;

typedef struct pset_setattr_args {
        psetid_t        pset;
        pset_attrtype_t type;
        pset_attrval_t  value;
} pset_setattr_t;

typedef struct pset_ctl_args {
        pset_request_t req;
        psetid_t pset;
        id_t     id;
} pset_ctl_t;

#endif
