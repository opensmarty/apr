/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#include "apr.h"
#include "apr_private.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "apr_portable.h"
#include "proc_mutex.h"
#include "misc.h"

static apr_status_t proc_mutex_cleanup(void *mutex_)
{
    apr_proc_mutex_t *mutex = mutex_;

    if (CloseHandle(mutex->handle) == 0) {
        return apr_get_os_error();
    }
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_proc_mutex_create(apr_proc_mutex_t **mutex,
                                                const char *fname,
                                                apr_lockmech_e mech,
                                                apr_pool_t *pool)
{
    HANDLE hMutex;
    SECURITY_ATTRIBUTES sec;
    sec.nLength = sizeof(SECURITY_ATTRIBUTES);
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle = TRUE;

    /* With Win2000 Terminal Services, the Mutex name can have a 
     * "Global\" or "Local\" prefix to explicitly create the object 
     * in the global or session name space.  Without Terminal Service
     * running on Win2000, Global\ and Local\ are ignored.  These
     * prefixes are only valid on Win2000+
     */
    if (fname) {
        if (apr_os_level >= APR_WIN_2000)
            fname = apr_pstrcat(pool, "Global\\", fname, NULL);
        else
            fname = apr_pstrdup(pool, fname);
    }

    hMutex = CreateMutex(&sec, FALSE, fname);
    if (!hMutex) {
	return apr_get_os_error();
    }

    *mutex = (apr_proc_mutex_t *)apr_palloc(pool, sizeof(apr_proc_mutex_t));
    (*mutex)->pool = pool;
    (*mutex)->handle = hMutex;
    (*mutex)->fname = fname;
    apr_pool_cleanup_register((*mutex)->pool, *mutex, 
                              proc_mutex_cleanup, apr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_proc_mutex_child_init(apr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    apr_pool_t *pool)
{
    HANDLE hMutex;

    if (apr_os_level >= APR_WIN_2000)
        fname = apr_pstrcat(pool, "Global\\", fname, NULL);
    else
        fname = apr_pstrdup(pool, fname);

    hMutex = OpenMutex(MUTEX_ALL_ACCESS, TRUE, fname);
    if (!hMutex) {
	return apr_get_os_error();
    }

    *mutex = (apr_proc_mutex_t *)apr_palloc(pool, sizeof(apr_proc_mutex_t));
    (*mutex)->pool = pool;
    (*mutex)->handle = hMutex;
    (*mutex)->fname = fname;
    apr_pool_cleanup_register((*mutex)->pool, *mutex, 
                              proc_mutex_cleanup, apr_pool_cleanup_null);
    return APR_SUCCESS;
}
    
APR_DECLARE(apr_status_t) apr_proc_mutex_lock(apr_proc_mutex_t *mutex)
{
    DWORD rv;

    rv = WaitForSingleObject(mutex->handle, INFINITE);

    if (rv == WAIT_OBJECT_0 || rv == WAIT_ABANDONED) {
        return APR_SUCCESS;
    }
    return apr_get_os_error();
}

APR_DECLARE(apr_status_t) apr_proc_mutex_trylock(apr_proc_mutex_t *mutex)
{
    DWORD rv;

    rv = WaitForSingleObject(mutex->handle, 0);

    if (rv == WAIT_OBJECT_0 || rv == WAIT_ABANDONED) {
        return APR_SUCCESS;
    }
    return apr_get_os_error();
}

APR_DECLARE(apr_status_t) apr_proc_mutex_unlock(apr_proc_mutex_t *mutex)
{
    if (ReleaseMutex(mutex->handle) == 0) {
        return apr_get_os_error();
    }
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_proc_mutex_destroy(apr_proc_mutex_t *mutex)
{
    apr_status_t stat;

    stat = proc_mutex_cleanup(mutex);
    if (stat == APR_SUCCESS) {
        apr_pool_cleanup_kill(mutex->pool, mutex, proc_mutex_cleanup);
    }
    return stat;
}

APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)

/* Implement OS-specific accessors defined in apr_portable.h */

APR_DECLARE(apr_status_t) apr_os_proc_mutex_get(apr_os_proc_mutex_t *ospmutex,
                                                apr_proc_mutex_t *mutex)
{
    *ospmutex = mutex->handle;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_os_proc_mutex_put(apr_proc_mutex_t **pmutex,
                                                apr_os_proc_mutex_t *ospmutex,
                                                apr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*pmutex) == NULL) {
        (*pmutex) = (apr_proc_mutex_t *)apr_palloc(pool,
                                                   sizeof(apr_proc_mutex_t));
        (*pmutex)->pool = pool;
    }
    (*pmutex)->handle = *ospmutex;
    return APR_SUCCESS;
}

