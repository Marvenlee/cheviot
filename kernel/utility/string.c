/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <kernel/types.h>
#include <kernel/utility.h>


/*
 * StrLen();
 */
 
size_t StrLen (const char *s)
{
    size_t len;
    
    for (len = 0; *s++ != '\0'; len++);
    return len;
}




/*
 * StrCmp();
 */

int StrCmp (const char *s, const char *t)
{
    while (*s == *t)
    {
        /* Should it not return the difference? */
    
        if (*s == '\0')
        {
            return *s - *t;
            /* return 0; */
        }
        
         s++;
         t++;
    }
    
    return *s - *t;
}




/* 
 * StrChr();
 */

char *StrChr (char *str, char ch)
{
    char *c = str;
    
    while (*c != '\0')
    {
        if (*c == ch)
            return c;
        
        c++;
    }
    
    return NULL;
}




/*
 * AtoI(); 
 *
 * FIXME:  Improve AtoI();
 *
 * Simple ASCII to Integer.  Doesn't handle signs or space
 * around number.
 *
 * Create StrToL/StrToUL ????????
 */

int AtoI (const char *str)
{
    const char *ch = str;
    int val = 0;
    
    while (*ch >= '0' &&  *ch <= '9')
    {
        val = (val * 10) + (*ch - '0');
        ch++;
    }
    
    return val;
}

