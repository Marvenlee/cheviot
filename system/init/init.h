/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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

#ifndef INIT_H
#define INIT_H


// Constants
#define ARG_SZ 256

// Globals
extern char **environ;

// Prototypes
int cmdStart (void);
int cmdChdir (void);
int cmdMknod (void);
int cmdWaitfor (void);
int cmdSleep (void);
int cmdSettty (void);
int cmdPivot (void);
int cmdRemount (void);
int cmdSetEnv(void);
char *tokenize(char *line);
char *readLine(void);
void PrintGreeting(void);


#endif
