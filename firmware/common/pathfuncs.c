/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2014 by Michael Sevakis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include "system.h"
#include "pathfuncs.h"
#include "string-extra.h"
#include <stdio.h>
#include "file_internal.h"
#include "debug.h"

// #define LOGF_ENABLE
#include "logf.h"



/* Just like path_strip_volume() but strips a leading drive specifier and
 * returns the drive number (A=0, B=1, etc.). -1 means no drive was found.
 * If 'greedy' is 'true', all separators after the volume are consumed.
 */
int path_strip_drive(const char *name, const char **nameptr, bool greedy)
{
    int c = toupper(*name);

    if (c >= 'A' && c <= 'Z' && name[1] == PATH_DRVSEPCH)
    {
        name = &name[2];
        if (greedy)
            name = GOBBLE_PATH_SEPCH(name);

        *nameptr = name;
        return c - 'A';
    }

    *nameptr = name;
    return -1;
}

/* Strips directory components from the path
 * ""      *nameptr->NUL,   len=0: ""
 * "/"     *nameptr->/,     len=1: "/"
 * "//"    *nameptr->2nd /, len=1: "/"
 * "/a"    *nameptr->a,     len=1: "a"
 * "a/"    *nameptr->a,     len=1: "a"
 * "/a/bc" *nameptr->b,     len=2: "bc"
 * "d"     *nameptr->d,     len=1: "d"
 * "ef/gh" *nameptr->g,     len=2: "gh"
 *
 * Notes: * Doesn't do suffix removal at this time.
 *        * In the same string, path_dirname() returns a pointer with the
 *          same or lower address as path_basename().
 *        * Pasting a separator between the returns of path_dirname() and
 *          path_basename() will result in a path equivalent to the input.
 */
size_t path_basename(const char *name, const char **nameptr)
{
    const char *p = name;
    const char *q = p;
    const char *r = q;

    while (*(p = GOBBLE_PATH_SEPCH(p)))
    {
        q = p;
        p = GOBBLE_PATH_COMP(++p);
        r = p;
    }

    if (r == name && p > name)
        q = p, r = q--; /* root - return last slash */
    /* else path is an empty string */

    *nameptr = q;
    return r - q;
}

/* Strips the trailing component from the path
 * ""      *nameptr->NUL,   len=0: ""
 * "/"     *nameptr->/,     len=1: "/"
 * "//"    *nameptr->2nd /, len=1: "/"
 * "/a"    *nameptr->/,     len=1: "/"
 * "a/"    *nameptr->a,     len=0: ""
 * "/a/bc" *nameptr->/,     len=2: "/a"
 * "d"     *nameptr->d,     len=0: ""
 * "ef/gh" *nameptr->e,     len=2: "ef"
 *
 * Notes: * Interpret len=0 as ".".
 *        * In the same string, path_dirname() returns a pointer with the
 *          same or lower address as path_basename().
 *        * Pasting a separator between the returns of path_dirname() and
 *          path_basename() will result in a path equivalent to the input.
 *
 */
size_t path_dirname(const char *name, const char **nameptr)
{
    const char *p = GOBBLE_PATH_SEPCH(name);
    const char *q = name;
    const char *r = p;

    while (*(p = GOBBLE_PATH_COMP(p)))
    {
        const char *s = p;

        if (!*(p = GOBBLE_PATH_SEPCH(p)))
            break;

        q = s;
    }

    if (q == name && r > name)
        name = r, q = name--; /* root - return last slash */

    *nameptr = name;
    return q - name;
}

/* Removes trailing separators from a path
 * ""     *nameptr->NUL,   len=0: ""
 * "/"    *nameptr->/,     len=1: "/"
 * "//"   *nameptr->2nd /, len=1: "/"
 * "/a/"  *nameptr->/,     len=2: "/a"
 * "//b/" *nameptr->1st /, len=3: "//b"
 * "/c/"  *nameptr->/,     len=2: "/c"
 */
size_t path_strip_trailing_separators(const char *name, const char **nameptr)
{
    const char *p;
    size_t len = path_basename(name, &p);

    if (len == 1 && *p == '/' && p > name)
    {
        *nameptr = p;
        name = p - 1; /* root with multiple separators */
    }
    else
    {
        *nameptr = name;
        p += len; /* length to end of basename */
    }

    return p - name;
}

/* Transforms "wrong" separators into the correct ones
 * "c:\windows\system32" -> "c:/windows/system32"
 *
 * 'path' and 'dstpath' may either be the same buffer or non-overlapping
 */
void path_correct_separators(char *dstpath, const char *path)
{
    char *dstp = dstpath;
    const char *p = path;

    while (1)
    {
        const char *next = strchr(p, PATH_BADSEPCH);
        if (!next)
            break;

        size_t size = next - p;

        if (dstpath != path)
            memcpy(dstp, p, size); /* not in-place */

        dstp += size;
        *dstp++ = PATH_SEPCH;
        p = next + 1;
    }

    if (dstpath != path)
        strcpy(dstp, p);
}

/* Remove dot segments from the path
 *
 * 'path' and 'dstpath' may either be the same buffer or non-overlapping
 */
void path_remove_dot_segments (char *dstpath, const char *path)
{
    char *dstp = dstpath;
    char *odstp = dstpath;
    const char *p = path;

    while (*p)
    {
        if (p[0] == '.' && p[1] == PATH_SEPCH)
            p += 2;
        else if (p[0] == '.' && p[1] == '.' && p[2] == PATH_SEPCH)
            p += 3;
        else if (p[0] == PATH_SEPCH && p[1] == '.' && p[2] == PATH_SEPCH)
            p += 2;
        else if (p[0] == PATH_SEPCH && p[1] == '.' && !p[2])
        {
            *dstp++ = PATH_SEPCH;
            break;
        }
        else if (p[0] == PATH_SEPCH && p[1] == '.' &&
            p[2] == '.' && p[3] == PATH_SEPCH)
        {
            dstp = odstp;
            p += 3;
        }
        else if (p[0] == PATH_SEPCH && p[1] == '.' && p[2] == '.' && !p[3])
        {
            dstp = odstp;
            *dstp++ = PATH_SEPCH;
            break;
        }
        else if (p[0] == '.' && !p[1])
            break;
        else if (p[0] == '.' && p[1] == '.' && !p[2])
            break;
        else
        {
            odstp = dstp;
            if (p[0] == PATH_SEPCH)
                *dstp++ = *p++;
            while (p[0] && p[0] != PATH_SEPCH)
                *dstp++ = *p++;
        }
    }
    *dstp = 0;
}

/* Appends one path to another, adding separators between components if needed.
 * basepath_max can be used to truncate the basepath if desired
 * NOTE: basepath is truncated after copying to the buffer so there must be enough
 * free space for the entirety of the basepath even if the resulting string would fit
 *
 * Return value and behavior is otherwise as strlcpy so that truncation may be
 * detected.
 *
 * For basepath and component:
 * PA_SEP_HARD adds a separator even if the base path is empty
 * PA_SEP_SOFT adds a separator only if the base path is not empty
 */
size_t path_append_ex(char *buf, const char *basepath, size_t basepath_max,
                   const char *component, size_t bufsize)
{
    size_t len = 0;
    bool separate = false;
    const char *base = basepath && basepath[0] ? basepath : buf;
    if (!base)
        return bufsize; /* won't work to get lengths from buf */

    if (!buf)
        bufsize = 0;

    if (path_is_absolute(component)) /* starts with a '/' path separator */
    {
            {
                /* 'component' is absolute; replace all */
                basepath = component;
                component = "";
                basepath_max = -1u;
            }
    }

    /* if basepath is not null or empty, buffer contents are replaced,
       otherwise buf contains the base path */

    if (base == buf)
        len = strlen(buf);
    else if (basepath)
    {
        /* strip extra leading separators */
        while (*basepath == PATH_SEPCH && *(basepath + 1) == PATH_SEPCH)
            basepath++;
        len = strlcpy(buf, basepath, bufsize);
        if (basepath_max < len) /*if needed truncate basepath to basepath_max */
        {
            len = basepath_max;
            buf[MIN(bufsize - 1, basepath_max)] = '\0';
        }
    }

    if (!basepath || !component || basepath_max == 0)
        separate = !len || base[len-1] != PATH_SEPCH;
    else if (component[0])
        separate = len && base[len-1] != PATH_SEPCH;

    /* caller might lie about size of buf yet use buf as the base */
    if (base == buf && bufsize && len >= bufsize)
        buf[bufsize - 1] = '\0';

    buf += len;
    bufsize -= MIN(len, bufsize);

    if (separate && (len++, bufsize > 0) && --bufsize > 0)
        *buf++ = PATH_SEPCH;

    return len + strlcpy(buf, component ?: "", bufsize);
}


size_t path_append(char *buf, const char *basepath,
                   const char *component, size_t bufsize)
{
    return path_append_ex(buf, basepath, -1u, component, bufsize);
}
/* Returns the location and length of the next path component, consuming the
 * input in the process.
 *
 * "/a/bc/d" breaks into:
 *   start:  *namep->1st /
 *   call 1: *namep->a,   *pathp->2nd / len=1: "a"
 *   call 2: *namep->b,   *pathp->3rd / len=2: "bc"
 *   call 3: *namep->d,   *pathp->NUL,  len=1: "d"
 *   call 4: *namep->NUL, *pathp->NUL,  len=0: ""
 *
 * Returns: 0 if the input has been consumed
 *          The length of the component otherwise
 */
ssize_t parse_path_component(const char **pathp, const char **namep)
{
    /* a component starts at a non-separator and continues until the next
       separator or null */
    const char *p = GOBBLE_PATH_SEPCH(*pathp);
    const char *name = p;

    if (*p)
        p = GOBBLE_PATH_COMP(++p);

    *pathp = p;
    *namep = name;

    return p - name;
}
