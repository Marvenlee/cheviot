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

#ifndef NDEBUG

#include <kernel/utility.h>
#include <kernel/dbg.h>


/*
 * Function prototype and Structure passed to DoPrintf();
 */

static void SnprintfPrintChar (char ch, void *arg);

struct SnprintfArg
{
    char *str;
    int size;
    int pos;
};





/*
 * Snprintf();
 */

int Snprintf (char *str, size_t size, const char *format, ...)
{
    va_list ap;

    struct SnprintfArg sa;
    
    va_start (ap, format);
    
    sa.str = str;
    sa.size = size;
    sa.pos = 0;

    DoPrintf (&SnprintfPrintChar, &sa, format, &ap);
    
    va_end (ap);
    
    return sa.pos - 1;
}




/*
 * Vsnprintf();
 */

int Vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    struct SnprintfArg sa;
        
    sa.str = str;
    sa.size = size;
    sa.pos = 0;

    DoPrintf (&SnprintfPrintChar, &sa, format, &args);
    
    return sa.pos - 1;
}




/*
 *
 */

static void SnprintfPrintChar (char ch, void *arg)
{
    struct SnprintfArg *sa;

    sa = (struct SnprintfArg *)arg;
    
    if (sa->pos < sa->size)
        *(sa->str + sa->pos) = ch;
    else if (sa->pos == sa->size)
        *(sa->str + sa->size) = '\0';
        
    sa->pos ++;
}














#endif


