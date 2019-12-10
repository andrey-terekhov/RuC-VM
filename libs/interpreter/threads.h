/*
 *	Copyright 2017 Andrey Terekhov
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

#ifndef H_THREADS
#define H_THREADS

#include <fcntl.h>


struct msg_info
{
	int numTh;
	int data;
};


void t_init();
void t_destroy();

int t_create_inner(void *(*func)(void *), void *arg);
int t_create(void *(*func)(void *));

int t_createDetached(void *(*func)(void *));

void t_exit();
void t_join(int numTh);
int t_getThNum();
void t_sleep(int miliseconds);

int t_sem_create(int level);
void t_sem_wait(int numSem);
void t_sem_post(int numSem);

void t_msg_send(struct msg_info msg);
struct msg_info t_msg_receive();

#endif
