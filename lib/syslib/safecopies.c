
/* Library functions to maintain internal data copying tables.
 *
 * April 21 2006: Initial version (Ben Gras)
 *
 */

#include <lib.h>
#include <errno.h>
#include <minix/sysutil.h>
#include <assert.h>
#include <stdlib.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/com.h>
#include <string.h>

#define ACCESS_CHECK(a) { 			\
	if((a) & ~(CPF_READ|CPF_WRITE)) {	\
		errno = EINVAL;			\
		return -1;			\
	}					\
   }

#define GID_CHECK(gid) {					\
	if(!GRANT_VALID(gid) || (gid) < 0 || (gid) >= ngrants) {\
		errno = EINVAL;					\
		return -1;					\
	}							\
   }

#define GID_CHECK_USED(gid) {					\
	GID_CHECK(gid);						\
	if(!(grants[gid].cp_flags & CPF_USED)) {		\
		errno = EINVAL;					\
		return -1;					\
	}							\
   }

PRIVATE cp_grant_t *grants = NULL;
PRIVATE int ngrants = 0, dynamic = 1;

PUBLIC int
cpf_preallocate(cp_grant_t *new_grants, int new_ngrants)
{
/* Use a statically allocated block of grants as our grant table.
 * This means we can't grow it dynamically any more.
 *
 * This function is used in processes that can't safely call realloc().
 */
	int s;

	/* If any table is already in place, we can't change it. */
	if(ngrants > 0) {
		errno = EBUSY;
		return -1;
	}

	/* Update kernel about the table. */
	if((s=sys_setgrant(new_grants, new_ngrants))) {
		return -1;
	}

	/* Update internal data. dynamic = 0 means no realloc()ing will be done
	 * and we can't grow beyond this size.
	 */
	grants = new_grants;
	ngrants = new_ngrants;
	dynamic = 0;

	return 0;
}

PRIVATE void
cpf_grow(void)
{
/* Grow the grants table if possible. If a preallocated block has been
 * submitted ('dynamic' is clear), we can't grow it. Otherwise, realloc().
 * Caller is expected to check 'ngrants' to see if the call was successful.
 */
	cp_grant_t *new_grants;
	cp_grant_id_t g;
	int new_size;

	/* Can't grow if static block already assigned. */
	if(!dynamic)
		return;

	new_size = (1+ngrants)*2;
	assert(new_size > ngrants);

	/* Allocate a block of new size. */
	if(!(new_grants=malloc(new_size * sizeof(grants[0]))))
		return;

	/* Copy old block to new block. */
	if(grants && ngrants > 0)
		memcpy(new_grants, grants, ngrants * sizeof(grants[0]));

	/* Make sure new slots are marked unused (CPF_USED is clear). */
	for(g = ngrants; g < new_size; g++)
		new_grants[g].cp_flags = 0;

	/* Inform kernel about new size (and possibly new location). */
	if((sys_setgrant(new_grants, new_size))) {
		free(new_grants);
		return;	/* Failed - don't grow then. */
	}

	/* Update internal data. */
	if(grants && ngrants > 0) free(grants);
	grants = new_grants;
	ngrants = new_size;
}

PRIVATE cp_grant_id_t
cpf_new_grantslot(void)
{
/* Find a new, free grant slot in the grant table, grow it if
 * necessary. If no free slot is found and the grow failed,
 * return -1. Otherwise, return grant slot number.
 */
	static cp_grant_id_t i = 0;
	cp_grant_id_t g;
	int n = 0;

	/* Any slots at all? */
	if(ngrants < 1) {
		errno = ENOSPC;
		return -1;
	}

	/* Find free slot. */
	for(g = i % ngrants;
	    n < ngrants && (grants[g].cp_flags & CPF_USED); n++)
		g = (g+1) % ngrants;

	/* Where to start searching next time. */
	i = g+1;

	assert(g <= ngrants);
	assert(n <= ngrants);

	/* No free slot found? */
	if(n == ngrants) {
		cpf_grow();
		assert(n <= ngrants); /* ngrants can't shrink. */
		if(n == ngrants) {
			/* ngrants hasn't increased. */
			errno = ENOSPC;
			return -1;
		}
		/* The new grant is the first available new grant slot. */
		g = n;
	}

	/* Basic sanity checks - if we get this far, g must be a valid,
	 * free slot.
	 */
	assert(GRANT_VALID(g));
	assert(g >= 0);
	assert(g < ngrants);
	assert(!(grants[g].cp_flags & CPF_USED));

	return g;
}

PUBLIC cp_grant_id_t
cpf_grant_direct(endpoint_t who_to, vir_bytes addr, size_t bytes, int access)
{
	cp_grant_id_t g;
	int r;
 
	/* Get new slot to put new grant in. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	assert(GRANT_VALID(g));
	assert(g >= 0);
	assert(g < ngrants);
	assert(!(grants[g].cp_flags & CPF_USED));

	if((r=cpf_setgrant_direct(g, who_to, addr, bytes, access)) < 0) {
		cpf_revoke(g);
		return GRANT_INVALID;
	}

	return g;
}

PUBLIC cp_grant_id_t
cpf_grant_indirect(endpoint_t who_to, endpoint_t who_from, cp_grant_id_t gr)
{
/* Grant process A access into process B. B has granted us access as grant
 * id 'gr'.
 */
	cp_grant_id_t g;
	int r;

	/* Obtain new slot. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	/* Basic sanity checks. */
	assert(GRANT_VALID(g));
	assert(g >= 0);
	assert(g < ngrants);
	assert(!(grants[g].cp_flags & CPF_USED));

	/* Fill in new slot data. */
	if((r=cpf_setgrant_indirect(g, who_to, who_from, gr)) < 0) {
		cpf_revoke(g);
		return GRANT_INVALID;
	}

	return g;
}

PUBLIC cp_grant_id_t
cpf_grant_magic(endpoint_t who_to, endpoint_t who_from,
	vir_bytes addr, size_t bytes, int access)
{
/* Grant process A access into process B. Not everyone can do this. */
	cp_grant_id_t g;
	int r;

	ACCESS_CHECK(access);

	/* Obtain new slot. */
	if((g = cpf_new_grantslot()) < 0)
		return -1;

	/* Basic sanity checks. */
	assert(GRANT_VALID(g));
	assert(g >= 0);
	assert(g < ngrants);
	assert(!(grants[g].cp_flags & CPF_USED));

	if((r=cpf_setgrant_magic(g, who_to, who_from, addr,
		bytes, access)) < 0) {
		cpf_revoke(g);
		return -1;
	}

	return g;
}

PUBLIC int
cpf_revoke(cp_grant_id_t g)
{
/* Revoke previously granted access, identified by grant id. */
	GID_CHECK_USED(g);

	/* Make grant invalid by setting flags to 0, clearing CPF_USED.
	 * This invalidates the grant.
	 */
	grants[g].cp_flags = 0;

	return 0;
}

PUBLIC int
cpf_lookup(cp_grant_id_t g, endpoint_t *granter, endpoint_t *grantee)
{
	/* First check slot validity, and if it's in use currently. */
	GID_CHECK_USED(g);

	if(grants[g].cp_flags & CPF_DIRECT) {
		if(granter) *granter = SELF;
		if(grantee) *grantee = grants[g].cp_u.cp_direct.cp_who_to;
	} else if(grants[g].cp_flags & CPF_MAGIC) {
		if(granter) *granter = grants[g].cp_u.cp_magic.cp_who_from;
		if(grantee) *grantee = grants[g].cp_u.cp_magic.cp_who_to;
	} else	return -1;

	return 0;
}

PUBLIC int
cpf_getgrants(grant_ids, n)
cp_grant_id_t *grant_ids;
int n;
{
	cp_grant_id_t g;
	int i;

	for(i = 0; i < n; i++) {
	  if((grant_ids[i] = cpf_new_grantslot()) < 0)
		break;
	  cpf_revoke(grant_ids[i]);
	}

	/* return however many grants were assigned. */
	return i;
}

PUBLIC int
cpf_setgrant_direct(gid, who, addr, bytes, access)
cp_grant_id_t gid;
endpoint_t who;
vir_bytes addr;
size_t bytes;
int access;
{
	GID_CHECK(gid);
	ACCESS_CHECK(access);

	grants[gid].cp_flags = access | CPF_DIRECT | CPF_USED;
	grants[gid].cp_u.cp_direct.cp_who_to = who;
	grants[gid].cp_u.cp_direct.cp_start = addr;
	grants[gid].cp_u.cp_direct.cp_len = bytes;

	return 0;
}

PUBLIC int
cpf_setgrant_indirect(gid, who_to, who_from, his_gid)
cp_grant_id_t gid;
endpoint_t who_to, who_from;
cp_grant_id_t his_gid;
{
	GID_CHECK(gid);

	/* Fill in new slot data. */
	grants[gid].cp_flags = CPF_USED | CPF_INDIRECT;
	grants[gid].cp_u.cp_indirect.cp_who_to = who_to;
	grants[gid].cp_u.cp_indirect.cp_who_from = who_from;
	grants[gid].cp_u.cp_indirect.cp_grant = his_gid;

	return 0;
}

PUBLIC int
cpf_setgrant_magic(gid, who_to, who_from, addr, bytes, access)
cp_grant_id_t gid;
endpoint_t who_to, who_from;
vir_bytes addr;
size_t bytes;
int access;
{
	GID_CHECK(gid);
	ACCESS_CHECK(access);

	/* Fill in new slot data. */
	grants[gid].cp_flags = CPF_USED | CPF_MAGIC | access;
	grants[gid].cp_u.cp_magic.cp_who_to = who_to;
	grants[gid].cp_u.cp_magic.cp_who_from = who_from;
	grants[gid].cp_u.cp_magic.cp_start = addr;
	grants[gid].cp_u.cp_magic.cp_len = bytes;

	return 0;
}

