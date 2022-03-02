/*
 *	Copyright 2014 Andrey Terekhov
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

#include "import.h"
#include "threads.h"
#include "utils.h"
#include <math.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 *	Я исхожу из того, что нумерация нитей процедурой t_create начинается с 1 и идет последовательно
 *	в соответствии с порядком вызовов этой процудуры, главная программа имеет номер 0. Если стандарт POSIX
 *	этого не обеспечивает, это должен сделать Саша Головань.
 *
 *	Память mem, как обычно, начинается с кода всей программы, включая нити, затем идут глобальные данные,
 *	затем куски для стеков и массивов гланой программы и нитей, каждый кусок имеет размер MAXMEMTHREAD.
 *
 *	Поскольку и при запуске главной прграммы, и при запуске любой нити процудура t_create получает в качестве
 *	параметра одну и ту же процедуру interpreter, важно, чтобы при начале работы программы или нити были
 *	правильно установлены l, x и pc, причем все важные переменные были локальными, тогда дальше все
 *	переключения между нитями будут заботой ОС.
 *
 *	Есть глобальный массив threads, i-ый элемент которого указывает на начало куска i-ой нити.
 *	Каждый кусок начинается с шапки, где хранятся l, x и pc, которые нужно установить в момент старта нити.
 */

#include "error.h"
#include "instuctions.h"


#define MAXREPRTAB	10000
#define MAXIDENTAB	10000
#define MAXMODETAB	1000
#define FUNCSIZE	100
#define MAXSTRINGL	128
#define INIPROSIZE	100

#define MAXMEMSIZE		100000
#define NUMOFTHREADS	10
#define MAXMEMTHREAD	MAXMEMSIZE / NUMOFTHREADS


enum TYPE
{
	TYPE_VOID			= -6,
	TYPE_FLOATING		= -3,
	TYPE_CHAR,
	TYPE_INTEGER,

	TYPE_STRUCTURE		= 1002,
	TYPE_ARRAY			= 1004,
};


int g, xx, iniproc, maxdisplg, wasmain;
int reprtab[MAXREPRTAB], rp, identab[MAXIDENTAB], id, modetab[MAXMODETAB], md;
int mem[MAXMEMSIZE], functions[FUNCSIZE], funcnum;
int threads[NUMOFTHREADS]; //, curthread, upcurthread;
int procd, iniprocs[INIPROSIZE], base = 0, adinit, NN;
FILE *input;

#ifdef __APPLE__
char sem_print[] = "sem_print", sem_debug[] = "sem_debug";
sem_t *sempr, *semdeb;
#else
sem_t sempr, semdeb;
#endif


#ifdef ROBOT
extern void send_int_to_robot(int, int, const int *);
extern void send_float_to_robot(int, int, const double *);
extern void send_string_to_robot(int, int, const char *);
extern int receive_int_from_robot(int);
extern double receive_float_from_robot(int);
// extern int *receive_string_from_robot(int);
#endif

void *interpreter(void *);


int szof(int type)
{
	return modetab[type] == TYPE_ARRAY
			   ? 1
			   : type == TYPE_FLOATING ? 2 : (type > 0 && modetab[type] == TYPE_STRUCTURE) ? modetab[type + 1] : 1;
}

void runtimeerr(int e, int i, int r)
{
	switch (e)
	{
		case index_out_of_range:
			printf(" индекс %i за пределами границ массива %i\n", i, r - 1);
			break;
		case wrong_kop:
			printf(" команду %i я пока не реализовал; номер нити = %i\n", i, r);
			break;
		case wrong_arr_init:
			printf(" массив с %i элементами инициализируется %i значениями\n", i, r);
			break;
		case wrong_string_init:
			printf(" строковая переменная с %i элементами инициализируется строкой с %i литерами\n", i, r);
			break;
		case wrong_number_of_elems:
			printf(" количество элементов в массиве по каждому измерению должно быть положительным, а тут %i\n", r);
			break;
		case zero_division:
			printf(" целое деление на 0\n");
			break;
		case float_zero_division:
			printf(" вещественное деление на 0\n");
			break;
		case mem_overflow:
			printf(" переполнение памяти, скорее всего, нет выхода из рекурсии\n");
			break;
		case sqrt_from_negat:
			printf(" попытка вычисления квадратного корня из отрицательного числа \n");
			break;
		case log_from_negat:
			printf(" попытка вычисления натурального логарифма из 0 или отрицательного числа\n");
			break;
		case log10_from_negat:
			printf(" попытка вычисления десятичного логарифма из 0 или отрицательного числа\n");
			break;
		case wrong_asin:
			printf(" аргумент арксинуса должен быть в отрезке [-1, 1]\n");
			break;
		case printf_runtime_crash:
			printf(" странно, printf не работает на этапе исполнения; ошибка коммпилятора");
			break;
		case init_err:
			printf(" количество элементов инициализации %i не совпадает с количеством элементов %i массива\n", i, r);
			break;
	}

#ifdef TESTING_EXIT_CODE
	exit(TESTING_EXIT_CODE);
#else
	exit(3);
#endif
}

/*
void prmem()
{
	int i;

	printf("mem=\n");
	for (i = g; i <= x; i++)
	{
		printf("%i ) %i\n",i, mem[i]);
	}
	printf("\n");
}
*/

void auxprintf(int strbeg, int databeg)
{
	int i, n = mem[strbeg - 1];
	int j;
	int curdata = databeg + 1;

	for (i = strbeg; i < strbeg + n; ++i)
	{
		if (mem[i] == '%')
		{
			switch (mem[++i])
			{
				case 'i':
				case 1094: // ц
					printf("%i", mem[curdata++]);
					break;

				case 'c':
				case 1083: // л
					printf_char(mem[curdata++]);
					break;

				case 'f':
				case 1074: // в
				{
					printf("%lf", *((double *)(&mem[curdata])));
					curdata += 2;
				}
				break;

				case 's':
				case 1089: // с
				{
					for (j = mem[curdata]; j - mem[curdata] < mem[mem[curdata] - 1]; ++j)
					{
						printf_char(mem[j]);
					}
					curdata++;
				}
				break;

				case '%':
					printf("%%");
					break;

				default:
					break;
			}
		}
		else
		{
			printf_char(mem[i]);
		}
	}
}

void auxprint(int beg, int t, char before, char after)
{
	double rf;
	int r;
	r = mem[beg];

	if (before)
	{
		printf("%c", before);
	}

	if (t == TYPE_INTEGER)
	{
		printf("%i", r);
	}
	else if (t == TYPE_CHAR)
	{
		printf_char(r);
	}
	else if (t == TYPE_FLOATING)
	{
		memcpy(&rf, &mem[beg], sizeof(double));
		printf("%20.15f", rf);
	}
	else if (t == TYPE_VOID)
	{
		printf(" значения типа ПУСТО печатать нельзя\n");
	}
	else if (modetab[t] == TYPE_ARRAY) // здесь t уже точно положительный
	{
		int rr = r;
		int i;
		int type = modetab[t + 1];
		int d;
		d = szof(type);

		if (type > 0)
		{
			for (i = 0; i < mem[rr - 1]; i++)
			{
				auxprint(rr + i * d, type, 0, '\n');
			}
		}
		else
		{
			for (i = 0; i < mem[rr - 1]; i++)
			{
				auxprint(rr + i * d, type, 0, (type == TYPE_CHAR ? 0 : ' '));
			}
		}
	}
	else if (modetab[t] == TYPE_STRUCTURE)
	{
		int cnt = modetab[t + 2];
		int i;
		printf("{");
		for (i = 2; i <= cnt; i += 2)
		{
			int type = modetab[t + i + 1];
			if (type < 0)
			{
				auxprint(beg, type, (i == 2 ? 0 : ' '), (i == cnt ? 0 : ','));
			}
			else
			{
				auxprint(beg, type, '\n', '\n');
			}
			beg += szof(type);
		}
		printf("}");
	}
	else
	{
		printf(" значения типа ФУНКЦИЯ и указателей печатать нельзя\n");
	}

	if (after)
	{
		printf("%c", after);
	}
}

void auxget(int beg, int t)
{
	double rf;
	// printf("beg=%i t=%i\n", beg, t);

	if (t == TYPE_INTEGER)
	{
		scanf(" %i", &mem[beg]);
	}
	else if (t == TYPE_CHAR)
	{
		mem[beg] = getf_char();
	}
	else if (t == TYPE_FLOATING)
	{
		scanf(" %lf", &rf);
		memcpy(&mem[beg], &rf, sizeof(double));
	}
	else if (t == TYPE_VOID)
	{
		printf(" значения типа ПУСТО вводить нельзя\n");
	}
	else if (modetab[t] == TYPE_ARRAY) // здесь t уже точно положительный
	{
		int rr = mem[beg];
		int type = modetab[t + 1];
		int i;
		int d;
		d = szof(type);

		for (i = 0; i < mem[rr - 1]; i++)
		{
			auxget(rr + i * d, type);
		}
	}
	else if (modetab[t] == TYPE_STRUCTURE)
	{
		int cnt = modetab[t + 2];
		int i;
		for (i = 2; i <= cnt; i += 2)
		{
			int type = modetab[t + i + 1];
			auxget(beg, type);
			beg += szof(type);
		}
	}
	else
	{
		printf(" значения типа ФУНКЦИЯ и указателей вводить нельзя\n");
	}
}

int check_zero_int(int r)
{
	if (r == 0)
	{
		runtimeerr(zero_division, 0, 0);
	}
	return r;
}

double check_zero_float(double r)
{
	if (r == 0)
	{
		runtimeerr(float_zero_division, 0, 0);
	}
	return r;
}

int dsp(int di, int l)
{
	return di < 0 ? g - di + 5 : l + di;
}

void *interpreter(void *pcPnt)
{
	int aux[] = { 3, 1, 2, 3 };
	int l;
	int x;
	int origpc = *((int *)pcPnt);
	int numTh = t_getThNum();
	int N;
	int bounds[100];
	int d;
	int from;
	int prtype;
	int cur0;
	int pc = abs(origpc);
	int i;
	int r;
	int flagstop = 1;
	int entry;
	int di;
	int di1;
	int len;
	int num;
	int a_str1;
	int str1;
	int str2;
	double lf;
	double rf;

	if (origpc > 0)
	{
		if (numTh)
		{
			threads[numTh] = cur0 = numTh * MAXMEMTHREAD;
			l = mem[threads[numTh]] = threads[numTh] + 2;
			x = mem[threads[numTh]] = l + mem[pc - 2]; // l + maxdispl
			mem[l + 2] = -1;
		}
		else
		{
			l = threads[0] + 2;
			x = mem[threads[0] + 1];
		}
	}
	else
	{
		l = mem[threads[numTh]];
		x = mem[threads[numTh] + 1];
	}

	flagstop = 1;
	while (flagstop)
	{
		memcpy(&rf, &mem[x - 1], sizeof(double));
		// printf("pc=%i mem[pc]=%i\n", pc, mem[pc]);
		// printf("running th #%i\n", t_getThNum());

		switch (mem[pc++])
		{
			case IC_STOP:
			{
				flagstop = 0;
				xx = x;
			}
			break;

			case IC_ASSERT:
			{
				int message = mem[x--];
				int cond = mem[x--];

				if (!cond)
				{
					printf("Test Faild\nMessage: ");

					for (int i = 0; i < mem[message - 1]; ++i)
					{
						printf("%c", (char)mem[message + i]);
					}

					printf("\n");
					fflush(stdout);
#ifdef TESTING_EXIT_CODE
					exit(TESTING_EXIT_CODE);
#else
					exit(1);
#endif
				}
			}
			break;

			case IC_CREATE_DIRECT:
			{
				int *arg = malloc(sizeof(*arg));
				*arg = pc;
				mem[++x] = t_create_inner(interpreter, (void *)arg);
			}
			break;

			case IC_CREATE:
			{
				int *arg = malloc(sizeof(*arg));
				*arg = mem[x];
				entry = functions[*arg > 0 ? *arg : mem[l - *arg]];
				*arg = entry + 3; // новый pc
				mem[x] = t_create_inner(interpreter, (void *)arg);
			}
			break;

			case IC_JOIN:
				t_join(mem[x--]);
				break;

			case IC_SLEEP:
				t_sleep(mem[x--]);
				break;

			case IC_EXIT_DIRECT:
			case IC_EXIT:
				t_exit();
				break;

			case IC_SEM_CREATE:
				mem[x] = t_sem_create(mem[x]);
				break;

			case IC_SEM_POST:
				t_sem_post(mem[x--]);
				break;

			case IC_SEM_WAIT:
				t_sem_wait(mem[x--]);
				break;

			case IC_INIT:
				t_init();
				break;

			case IC_DESTROY:
				t_destroy();
				break;

			case IC_MSG_RECEIVE:
			{
				struct msg_info m = t_msg_receive();
				mem[++x] = m.numTh;
				mem[++x] = m.data;
			}
			break;

			case IC_MSG_SEND:
			{
				struct msg_info m;
				m.data = mem[x--];
				m.numTh = mem[x--];
				t_msg_send(m);
			}
			break;

			case IC_GETNUM:
				mem[++x] = numTh;
				break;

#ifdef ROBOT
			case SEND_INTC:
			{
				int array = mem[x--];
				int type = mem[x--];
				int size = mem[array - 1];

				send_int_to_robot(type, size, &mem[array]);
			}
			break;
			case SEND_FLOATC:
			{
				int array = mem[x--];
				int type = mem[x--];
				int size = mem[array - 1];

				send_float_to_robot(type, size, (double *)&mem[array]);
			}
			break;
			case SEND_STRINGC:
			{
				int array = mem[x--];
				int type = mem[x--];
				int size = mem[array - 1];

				char str[MAXSTRINGL];

				for (int i = 0; i < size; i++)
				{
					str[i] = mem[array + i];
				}
				str[size] = '\0';

				send_string_to_robot(type, size, str);
			}
			break;

			case RECEIVE_INTC:
			{
				mem[x] = receive_int_from_robot(mem[x]);
			}
			break;
			case RECEIVE_FLOATC:
			{
				double temp = receive_float_from_robot(mem[x]);
				memcpy(&mem[x], &temp, sizeof(double));
				x++;
			}
			break;
			case RECEIVE_STRINGC:
				break;
#endif

			case IC_FUNC_BEG:
				pc = mem[pc + 1];
				break;

			case IC_PRINT:
			{
				int t;
#ifdef __APPLE__
				sem_wait(sempr);
#else
				sem_wait(&sempr);
#endif
				t = mem[pc++];
				x -= szof(t);
				auxprint(x + 1, t, 0, '\n');
				fflush(stdout);
#ifdef __APPLE__
				sem_post(sempr);
#else
				sem_post(&sempr);
#endif
			}
			break;

			case IC_PRINTID:
			{
#ifdef __APPLE__
				sem_wait(sempr);
#else
				sem_wait(&sempr);
#endif
				i = mem[pc++]; // ссылка на identtab
				prtype = identab[i + 2];
				r = identab[i + 1] + 2; // ссылка на reprtab

				do
				{
					printf_char(reprtab[r++]);
				} while (reprtab[r] != 0);

				if (prtype > 0 && modetab[prtype] == TYPE_ARRAY && modetab[prtype + 1] > 0)
				{
					auxprint(dsp(identab[i + 3], l), prtype, '\n', '\n');
				}
				else
				{
					auxprint(dsp(identab[i + 3], l), prtype, ' ', '\n');
				}

				fflush(stdout);
#ifdef __APPLE__
				sem_post(sempr);
#else
				sem_post(&sempr);
#endif
			}
			break;

			/*
			 *	Ожидает указатель на форматную строку на верхушке стека
			 *	Принимает единственным параметром суммарный размер того, что нужно напечатать
			 *	Проверок на типы не делает, этим занимался компилятор
			 *	Если захотим передавать динамически формируемые строки, нужно будет откуда-то брать весь набор типов
			 *	печатаемого
			 */
			case IC_PRINTF:
			{
				int sumsize, strbeg;

#ifdef __APPLE__
				sem_wait(sempr);
#else
				sem_wait(&sempr);
#endif
				sumsize = mem[pc++];
				strbeg = mem[x--];

				auxprintf(strbeg, x -= sumsize);
				fflush(stdout);
#ifdef __APPLE__
				sem_post(sempr);
#else
				sem_post(&sempr);
#endif
			}
			break;

			case IC_GETID:
			{
#ifdef __APPLE__
				sem_wait(sempr);
#else
				sem_wait(&sempr);
#endif
				i = mem[pc++]; // ссылка на identtab
				prtype = identab[i + 2];
				r = identab[i + 1] + 2; // ссылка на reprtab

				do
				{
					printf_char(reprtab[r++]);
				} while (reprtab[r] != 0);
				printf(" ");
				fflush(stdout);

				auxget(dsp(identab[i + 3], l), prtype);
#ifdef __APPLE__
				sem_post(sempr);
#else
				sem_post(&sempr);
#endif
			}
			break;

			case IC_ABSI:
				mem[x] = abs(mem[x]);
				break;
			case IC_ABS:
			{
				rf = fabs(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_SQRT:
			{
				if (rf < 0)
				{
					runtimeerr(sqrt_from_negat, 0, 0);
				}
				rf = sqrt(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_EXP:
			{
				rf = exp(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_SIN:
			{
				rf = sin(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_COS:
			{
				rf = cos(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_LOG:
			{
				if (rf <= 0)
				{
					runtimeerr(log_from_negat, 0, 0);
				}
				rf = log(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_LOG10:
			{
				if (rf <= 0)
				{
					runtimeerr(log10_from_negat, 0, 0);
				}
				rf = log10(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_ASIN:
			{
				if (rf < -1 || rf > 1)
				{
					runtimeerr(wrong_asin, 0, 0);
				}
				rf = asin(rf);
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_RAND:
			{
				rf = (double)rand() / RAND_MAX;
				memcpy(&mem[++x], &rf, sizeof(double));
				++x;
			}
			break;
			case IC_ROUND:
				mem[--x] = rf < 0 ? (int)(rf - 0.5) : (int)(rf + 0.5);
				break;

			case IC_STRCPY:
			{
				str2 = mem[x--];
				a_str1 = mem[x--];
				mem[a_str1] = str2;
			}
			break;
			case IC_STRNCPY:
			{
				num = mem[x--];
				str2 = mem[x--];
				a_str1 = mem[x--];

				if (num > mem[str2 - 1])
				{
#ifdef TESTING_EXIT_CODE
					exit(TESTING_EXIT_CODE);
#else
					exit(2); // error
#endif
				}

				if (num <= mem[mem[a_str1] - 1])
				{
					a_str1 = mem[a_str1];
					mem[a_str1 - 1] = num;
					num += a_str1;
					while (a_str1 < num)
					{
						mem[a_str1++] = mem[str2++];
					}
				}
				mem[x++] = num;
				mem[a_str1] = x;

				num += x;
				while (x < num)
				{
					mem[x++] = mem[str2++];
				}
				x--;
			}
			break;
			case IC_STRCAT:
			{
				str2 = mem[x--];
				a_str1 = mem[x--];
				str1 = mem[a_str1];

				mem[x++] = mem[str2 - 1] + mem[mem[a_str1] - 1];
				mem[a_str1] = x;
				a_str1 = mem[a_str1];

				num = x + mem[str1 - 1];
				while (x < num)
				{
					mem[x++] = mem[str1++];
				}

				num = x + mem[str2 - 1];
				while (x < num)
				{
					mem[x++] = mem[str2++];
				}
				x--;
			}
			break;
			case IC_STRNCAT:
			{
				num = mem[x--];
				str2 = mem[x--];
				a_str1 = mem[x--];
				str1 = mem[a_str1];

				mem[x++] = num + mem[mem[a_str1] - 1];
				mem[a_str1] = x;
				a_str1 = mem[a_str1];

				i = x + mem[str1 - 1];
				while (x < i)
				{
					mem[x++] = mem[str1++];
				}

				num += x;
				while (x < num)
				{
					mem[x++] = mem[str2++];
				}
				x--;
			}
			break;
			case IC_STRCMP:
			{
				str2 = mem[x--];
				a_str1 = mem[x];

				if (mem[a_str1 - 1] < mem[str2 - 1])
				{
					mem[x] = 1;
					break;
				}
				else if (mem[a_str1 - 1] > mem[str2 - 1])
				{
					mem[x] = -1;
					break;
				}
				else
				{
					for (i = 0; i < mem[str2 - 1]; i++)
					{
						if (mem[a_str1 + i] < mem[str2 + i])
						{
							mem[x] = 1;
							break;
						}
						else if (mem[a_str1 + i] > mem[str2 + i])
						{
							mem[x] = -1;
							break;
						}
					}

					if (i == mem[str2 - 1])
					{
						mem[x] = 0;
					}
				}
			}
			break;
			case IC_STRNCMP:
			{
				num = mem[x--];
				str2 = mem[x--];
				a_str1 = mem[x];

				if (mem[a_str1 - 1] < mem[str2 - 1] && mem[a_str1 - 1] < num)
				{
					mem[x] = 1;
					break;
				}
				else if (mem[a_str1 - 1] > mem[str2 - 1] && mem[str2 - 1] < num)
				{
					mem[x] = -1;
					break;
				}
				else
				{
					for (i = 0; i < num; i++)
					{
						if (i == mem[a_str1 - 1] && i == mem[str2 - 1])
						{
							mem[x] = 0;
							break;
						}

						if (mem[a_str1 + i] < mem[str2 + i])
						{
							mem[x] = 1;
							break;
						}

						if (mem[a_str1 + i] > mem[str2 + i])
						{
							mem[x] = -1;
							break;
						}
					}

					if (i == num)
					{
						mem[x] = 0;
					}
				}
			}
			break;
			case IC_STRSTR:
			{
				int j, flag = 0;
				str2 = mem[x--];
				a_str1 = mem[x];

				for (i = 0; i < mem[a_str1 - 1] - mem[str2 - 1]; i++)
				{
					if (mem[str2] == mem[a_str1 + i])
					{
						for (j = 0; j < mem[str2 - 1]; j++)
						{
							if (mem[str2 + j] != mem[a_str1 + i + j])
							{
								flag = 1;
								break;
							}
						}

						if (flag == 0)
						{
							mem[x] = i + 1;
							break;
						}
						flag = 0;
					}
				}

				if (i >= mem[a_str1 - 1] - mem[str2 - 1])
				{
					mem[x] = -1;
					break;
				}
			}
			break;
			case IC_STRLEN:
			{
				a_str1 = mem[x];
				mem[x] = mem[a_str1 - 1];
			}
			break;
			case IC_STRUCT_WITH_ARR:
			{
				int oldpc, oldbase = base, procnum;
				base = dsp(mem[pc++], l);
				procnum = mem[pc++];
				oldpc = pc;
				pc = -procnum;
				mem[threads[numTh] + 1] = x;

				interpreter((void *)&pc);

				x = xx;
				pc = oldpc;
				base = oldbase;
				flagstop = 1;
			}
			break;
			case IC_DEFARR: // N, d, displ, proc	на стеке N1, N2, ... , NN
			{
				int N = mem[pc++];
				int d = mem[pc++];
				int curdsp = mem[pc++];
				int proc = mem[pc++];
				int usual = mem[pc++];
				int all = mem[pc++];
				int instruct = mem[pc++];

				int stackC0[10], stacki[10], i, curdim = 1;
				if (usual >= 2)
				{
					usual -= 2;
				}

				NN = mem[x]; // будет использоваться в ARRINIT только при usual=1
				for (i = usual && all ? N + 1 : N; i > 0; i--)
				{
					if ((bounds[i] = mem[x--]) <= 0)
					{
						runtimeerr(wrong_number_of_elems, 0, bounds[i]);
					}
				}

				if (N > 0)
				{
					stacki[1] = 0;
					mem[++x] = bounds[1];
					mem[instruct ? base + curdsp : dsp(curdsp, l)] = stackC0[1] = x + 1;
					x += bounds[1] * (curdim < abs(N) ? 1 : d);

					if (x >= threads[numTh] + MAXMEMTHREAD)
					{
						runtimeerr(mem_overflow, 0, 0);
					}

					if (N == 1)
					{
						if (proc)
						{
							int curx = x, oldbase = base, oldpc = pc, i;
							for (i = stackC0[1]; i <= curx; i += d)
							{
								pc = -proc; // вычисление границ очередного массива в структуре
								base = i;
								mem[threads[numTh] + 1] = x;

								interpreter((void *)&pc);

								flagstop = 1;
								x = xx;
							}
							pc = oldpc;
							base = oldbase;
						}
					}
					else
					{
						while (curdim != 0)
						{
							while (stacki[curdim] < bounds[curdim])
							{
								// next elem
								do
								{
									// go down
									mem[++x] = bounds[curdim + 1];
									mem[stackC0[curdim] + stacki[curdim]++] = stackC0[curdim + 1] = x + 1;
									x += bounds[curdim + 1] * (curdim == N - 1 ? d : 1);

									if (x >= threads[numTh] + MAXMEMTHREAD)
									{
										runtimeerr(mem_overflow, 0, 0);
									}
									++curdim;
									stacki[curdim] = 0;
								} while (curdim < N);
								// построена очередная вертикаль подмассивов

								if (proc)
								{
									int curx = x, oldbase = base, oldpc = pc, i;
									for (i = stackC0[curdim]; i <= curx; i += d)
									{
										pc = proc; // вычисление границ очередного массива в структуре
										base = i;
										mem[threads[numTh] + 1] = x;

										interpreter((void *)&pc);

										flagstop = 1;
										x = xx;
									}
									pc = oldpc;
									base = oldbase;
								}
								// go right
								--curdim;
							}
							--curdim;
						}
					}
				}
				adinit = x + 1; // при usual == 1 использоваться не будет
			}
			break;
			case IC_BEG_INIT:
				mem[++x] = mem[pc++];
				break;
			/*
			case STRUCTINIT:
				pc++;
				break;
			*/
			case IC_STRING_INIT:
			{
				di = mem[pc++];
				r = mem[di < 0 ? g - di : l + di];
				N = mem[r - 1];
				from = mem[x--];
				d = mem[from - 1]; // d - кол-во литер в строке-инициаторе

				if (N != d)
				{
					runtimeerr(wrong_string_init, N, d);
				}
				for (i = 0; i < N; i++)
				{
					mem[r + i] = mem[from + i];
				}
			}
			break;
			case IC_ARR_INIT:
			{
				N = mem[pc++]; // N - размерность
				d = mem[pc++]; // d - шаг

				int addr = dsp(mem[pc++], l);
				int usual = mem[pc++];
				int onlystrings = usual >= 2 ? usual -= 2, 1 : 0;
				int stA[10], stN[10], sti[10], stpnt = 1, oldx = adinit;

				if (N == 1)
				{
					if (onlystrings)
					{
						mem[addr] = mem[x--];
					}
					else
					{
						mem[addr] = adinit + 1;

						if (usual && mem[adinit] != NN) // здесь usual == 1,
						{								// если usual == 0, проверка не нужна
							runtimeerr(init_err, mem[adinit], NN);
						}
						adinit += mem[adinit] * d + 1;
					}
				}
				else
				{
					stA[1] = mem[addr]; // массив самого верхнего уровня
					stN[1] = mem[stA[1] - 1];
					sti[1] = 0;

					if (mem[adinit] != stN[1])
					{
						runtimeerr(init_err, mem[adinit], stN[1]);
					}

					adinit++;
					do
					{
						while (stpnt < N - 1)
						{
							stA[stpnt + 1] = mem[stA[stpnt]];
							sti[++stpnt] = 0;
							stN[stpnt] = mem[stA[stpnt] - 1];
							if (mem[adinit] != stN[stpnt])
							{
								runtimeerr(init_err, mem[adinit], stN[stpnt]);
							}
							adinit++;
						}

						do
						{
							if (onlystrings)
							{
								mem[stA[stpnt] + sti[stpnt]] = mem[++oldx];
								if (usual && mem[mem[oldx - 1] - 1] != NN)
								{
									runtimeerr(init_err, mem[adinit], NN);
								}
							}
							else
							{
								if (usual && mem[adinit] != NN)
								{
									runtimeerr(init_err, mem[adinit], NN);
								}
								mem[stA[stpnt] + sti[stpnt]] = adinit + 1;
								adinit += mem[adinit] * d + 1;
							}
						} while (++sti[stpnt] < stN[stpnt]);

						if (stpnt > 1)
						{
							sti[stpnt] = 0;
							stpnt--;
							stA[stpnt] += ++sti[stpnt];
						}
					} while (stpnt != 1 || sti[1] != stN[1]);
				}
				x = adinit - 1;
			}
			break;

			case IC_LI:
				mem[++x] = mem[pc++];
				break;
			case IC_LID:
			{
				memcpy(&mem[++x], &mem[pc++], sizeof(double));
				++x;
				++pc;
			}
			break;
			case IC_LOAD:
				mem[++x] = mem[dsp(mem[pc++], l)];
				break;
			case IC_LOADD:
			{
				memcpy(&mem[++x], &mem[dsp(mem[pc++], l)], sizeof(double));
				++x;
			}
			break;
			case IC_LAT:
				mem[x] = mem[mem[x]];
				break;
			case IC_LATD:
			{
				memcpy(&rf, &mem[mem[x]], sizeof(double));
				memcpy(&mem[x++], &rf, sizeof(double));
			}
			break;
			case IC_LA:
				mem[++x] = dsp(mem[pc++], l);
				break;
			case IC_CALL1:
			{
				mem[l + 1] = ++x;
				mem[x++] = l;
				mem[x++] = 0; // следующая статика
				mem[x] = 0;	  // pc в момент вызова
			}
			break;
			case IC_CALL2:
			{
				i = mem[pc++];
				entry = functions[i > 0 ? i : mem[l - i]];

				l = mem[l + 1];
				x = l + mem[entry + 1] - 1;

				if (x >= threads[numTh] + MAXMEMTHREAD)
				{
					runtimeerr(mem_overflow, 0, 0);
				}
				mem[l + 2] = pc;
				pc = entry + 3;
			}
			break;
			case IC_RETURN_VAL:
			{
				d = mem[pc++];
				pc = mem[l + 2];

				if (pc == -1) // конец нити
				{
					flagstop = 0;
				}
				else
				{
					r = l;
					l = mem[l];
					mem[l + 1] = 0;
					from = x - d;
					x = r - 1;

					for (i = 0; i < d; i++)
					{
						mem[++x] = mem[++from];
					}
				}
			}
			break;
			case IC_RETURN_VOID:
			{
				pc = mem[l + 2];

				if (pc == -1) // конец нити
				{
					flagstop = 0;
				}
				else
				{
					x = l - 1;
					l = mem[l];
					mem[l + 1] = 0;
				}
			}
			break;
			case IC_NOP:
				break;
			case IC_B:
			case IC_STRING:
				pc = mem[pc];
				break;
			case IC_BE0:
				pc = (mem[x--]) ? pc + 1 : mem[pc];
				break;
			case IC_BNE0:
				pc = (mem[x--]) ? mem[pc] : pc + 1;
				break;
			case IC_UPB:
			{
				from = mem[x--];
				N = mem[x];

				for (i = 0; i < N; i++)
				{
					from = mem[from];
				}
				mem[x] = mem[from - 1];
			}
			break;
			case IC_SELECT:
				mem[x] += mem[pc++]; // ident displ
				break;
			case IC_COPY00:
			{
				di = dsp(mem[pc++], l);
				di1 = dsp(mem[pc++], l);
				len = mem[pc++];

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[di1 + i];
				}
			}
			break;
			case IC_COPY01:
			{
				di = dsp(mem[pc++], l);
				len = mem[pc++];
				di1 = mem[x--];

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[di1 + i];
				}
			}
			break;
			case IC_COPY10:
			{
				di = mem[x--];
				di1 = dsp(mem[pc++], l);
				len = mem[pc++];

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[di1 + i];
				}
			}
			break;
			case IC_COPY11:
			{
				di1 = mem[x--];
				di = mem[x--];
				len = mem[pc++];

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[di1 + i];
				}
			}
			break;
			case IC_COPY0ST:
			{
				di = dsp(mem[pc++], l);
				len = mem[pc++];

				for (i = 0; i < len; i++)
				{
					mem[++x] = mem[di + i];
				}
			}
			break;
			case IC_COPY1ST:
			{
				di = mem[x--];
				len = mem[pc++];

				for (i = 0; i < len; i++)
				{
					mem[++x] = mem[di + i];
				}
			}
			break;
			case IC_COPY0ST_ASSIGN:
			{
				di = dsp(mem[pc++], l);
				len = mem[pc++];
				x -= len;

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[x + i + 1];
				}
				x += len;
			}
			break;
			case IC_COPY1ST_ASSIGN:
			{
				len = mem[pc++];
				x -= len;
				di = mem[x--];

				for (i = 0; i < len; i++)
				{
					mem[di + i] = mem[x + i + 2];
				}
			}
			break;
			case IC_COPYST:
			{
				di = mem[pc++];		// смещ поля
				len = mem[pc++];	// длина поля
				x -= mem[pc++];		// длина всей структуры

				for (i = 1; i <= len; i++)
				{
					mem[x + i] = mem[x + i + di];
				}
				x += len;
			}
			break;

			case IC_SLICE:
			{
				d = mem[pc++];
				i = mem[x--]; // index
				r = mem[x];	  // array

				if (i < 0 || i >= mem[r - 1])
				{
					runtimeerr(index_out_of_range, i, mem[r - 1]);
				}
				mem[x] = r + i * d;
			}
			break;
			case IC_WIDEN:
			{
				rf = (double)mem[x];
				memcpy(&mem[x++], &rf, sizeof(double));
			}
			break;
			case IC_WIDEN1:
			{
				mem[x + 1] = mem[x];
				mem[x] = mem[x - 1];
				rf = (double)mem[x - 2];
				memcpy(&mem[x - 2], &rf, sizeof(double));
				++x;
			}
			break;
			case IC_DUPLICATE:
			{
				r = mem[x];
				mem[++x] = r;
			}
			break;

			case IC_ROWING: // ROWING
				mem[g + 3] = mem[x];
				mem[x] = g + 3;
				break;

			case IC_ROWING_D: // ROWINGD
				mem[g + 5] = mem[x - 1];
				mem[g + 6] = mem[x];
				mem[--x] = g + 5;
				break;

			case IC_ASSIGN:
				mem[dsp(mem[pc++], l)] = mem[x];
				break;
			case IC_REM_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] %= check_zero_int(mem[x]);
				mem[x] = r;
			}
			break;
			case IC_SHL_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] <<= mem[x];
				mem[x] = r;
			}
			break;
			case IC_SHR_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] >>= mem[x];
				mem[x] = r;
			}
			break;
			case IC_AND_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] &= mem[x];
				mem[x] = r;
			}
			break;
			case IC_XOR_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] ^= mem[x];
				mem[x] = r;
			}
			break;
			case IC_OR_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] |= mem[x];
				mem[x] = r;
			}
			break;
			case IC_ADD_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] += mem[x];
				mem[x] = r;
			}
			break;
			case IC_SUB_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] -= mem[x];
				mem[x] = r;
			}
			break;
			case IC_MUL_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] *= mem[x];
				mem[x] = r;
			}
			break;
			case IC_DIV_ASSIGN:
			{
				r = mem[dsp(mem[pc++], l)] /= check_zero_int(mem[x]);
				mem[x] = r;
			}
			break;

			case IC_ASSIGN_V:
				mem[dsp(mem[pc++], l)] = mem[x--];
				break;
			case IC_REM_ASSIGN_V:
				mem[dsp(mem[pc++], l)] %= check_zero_int(mem[x--]);
				break;
			case IC_SHL_ASSIGN_V:
				mem[dsp(mem[pc++], l)] <<= mem[x--];
				break;
			case IC_SHR_ASSIGN_V:
				mem[dsp(mem[pc++], l)] >>= mem[x--];
				break;
			case IC_AND_ASSIGN_V:
				mem[dsp(mem[pc++], l)] &= mem[x--];
				break;
			case IC_XOR_ASSIGN_V:
				mem[dsp(mem[pc++], l)] ^= mem[x--];
				break;
			case IC_OR_ASSIGN_V:
				mem[dsp(mem[pc++], l)] |= mem[x--];
				break;
			case IC_ADD_ASSIGN_V:
				mem[dsp(mem[pc++], l)] += mem[x--];
				break;
			case IC_SUB_ASSIGN_V:
				mem[dsp(mem[pc++], l)] -= mem[x--];
				break;
			case IC_MUL_ASSIGN_V:
				mem[dsp(mem[pc++], l)] *= mem[x--];
				break;
			case IC_DIV_ASSIGN_V:
				mem[dsp(mem[pc++], l)] /= check_zero_int(mem[x--]);
				break;

			case IC_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] = mem[x];
				mem[--x] = r;
			}
			break;
			case IC_REM_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] %= check_zero_int(mem[x]);
				mem[--x] = r;
			}
			break;
			case IC_SHL_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] <<= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_SHR_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] >>= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_AND_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] &= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_XOR_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] ^= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_OR_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] |= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_ADD_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] += mem[x];
				mem[--x] = r;
			}
			break;
			case IC_SUB_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] -= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_MUL_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] *= mem[x];
				mem[--x] = r;
			}
			break;
			case IC_DIV_ASSIGN_AT:
			{
				r = mem[mem[x - 1]] /= check_zero_int(mem[x]);
				mem[--x] = r;
			}
			break;

			case IC_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] = mem[x];
				x -= 2;
			}
			break;
			case IC_REM_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] %= check_zero_int(mem[x]);
				x -= 2;
			}
			break;
			case IC_SHL_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] <<= mem[x];
				x -= 2;
			}
			break;
			case IC_SHR_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] >>= mem[x];
				x -= 2;
			}
			break;
			case IC_AND_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] &= mem[x];
				x -= 2;
			}
			break;
			case IC_XOR_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] ^= mem[x];
				x -= 2;
			}
			break;
			case IC_OR_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] |= mem[x];
				x -= 2;
			}
			break;
			case IC_ADD_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] += mem[x];
				x -= 2;
			}
			break;
			case IC_SUB_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] -= mem[x];
				x -= 2;
			}
			break;
			case IC_MUL_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] *= mem[x];
				x -= 2;
			}
			break;
			case IC_DIV_ASSIGN_AT_V:
			{
				mem[mem[x - 1]] /= check_zero_int(mem[x]);
				x -= 2;
			}
			break;

			case IC_LOG_OR:
			{
				mem[x - 1] = mem[x - 1] || mem[x];
				x--;
			}
			break;
			case IC_LOG_AND:
			{
				mem[x - 1] = mem[x - 1] && mem[x];
				x--;
			}
			break;
			case IC_OR:
			{
				mem[x - 1] |= mem[x];
				x--;
			}
			break;
			case IC_XOR:
			{
				mem[x - 1] ^= mem[x];
				x--;
			}
			break;
			case IC_AND:
			{
				mem[x - 1] &= mem[x];
				x--;
			}
			break;
			case IC_SHR:
			{
				mem[x - 1] >>= mem[x];
				x--;
			}
			break;
			case IC_SHL:
			{
				mem[x - 1] <<= mem[x];
				x--;
			}
			break;
			case IC_REM:
			{
				mem[x - 1] %= mem[x];
				x--;
			}
			break;
			case IC_EQ:
			{
				mem[x - 1] = mem[x - 1] == mem[x];
				x--;
			}
			break;
			case IC_NE:
			{
				mem[x - 1] = mem[x - 1] != mem[x];
				x--;
			}
			break;
			case IC_LT:
			{
				mem[x - 1] = mem[x - 1] < mem[x];
				x--;
			}
			break;
			case IC_GT:
			{
				mem[x - 1] = mem[x - 1] > mem[x];
				x--;
			}
			break;
			case IC_LE:
			{
				mem[x - 1] = mem[x - 1] <= mem[x];
				x--;
			}
			break;
			case IC_GE:
			{
				mem[x - 1] = mem[x - 1] >= mem[x];
				x--;
			}
			break;
			case IC_ADD:
			{
				mem[x - 1] += mem[x];
				x--;
			}
			break;
			case IC_SUB:
			{
				mem[x - 1] -= mem[x];
				x--;
			}
			break;
			case IC_MUL:
			{
				mem[x - 1] *= mem[x];
				x--;
			}
			break;
			case IC_DIV:
			{
				mem[x - 1] /= check_zero_int(mem[x]);
				x--;
			}
			break;
			case IC_POST_INC:
			{
				mem[++x] = mem[r = dsp(mem[pc++], l)];
				mem[r]++;
			}
			break;
			case IC_POST_DEC:
			{
				mem[++x] = mem[r = dsp(mem[pc++], l)];
				mem[r]--;
			}
			break;
			case IC_PRE_INC:
				mem[++x] = ++mem[dsp(mem[pc++], l)];
				break;
			case IC_PRE_DEC:
				mem[++x] = --mem[dsp(mem[pc++], l)];
				break;
			case IC_POST_INC_AT:
			{
				mem[x] = mem[r = mem[x]];
				mem[r]++;
			}
			break;
			case IC_POST_DEC_AT:
			{
				mem[x] = mem[r = mem[x]];
				mem[r]--;
			}
			break;
			case IC_PRE_INC_AT:
				mem[x] = ++mem[mem[x]];
				break;
			case IC_PRE_DEC_AT:
				mem[x] = --mem[mem[x]];
				break;
			case IC_PRE_INC_V:
			case IC_POST_INC_V:
				mem[dsp(mem[pc++], l)]++;
				break;
			case IC_PRE_DEC_V:
			case IC_POST_DEC_V:
				mem[dsp(mem[pc++], l)]--;
				break;
			case IC_PRE_INC_AT_V:
			case IC_POST_INC_AT_V:
				mem[mem[x--]]++;
				break;
			case IC_PRE_DEC_AT_V:
			case IC_POST_DEC_AT_V:
				mem[mem[x--]]--;
				break;

			case IC_UNMINUS:
				mem[x] = -mem[x];
				break;

			case IC_ASSIGN_R:
			{
				mem[r = dsp(mem[pc++], l)] = mem[x - 1];
				mem[r + 1] = mem[x];
			}
			break;
			case IC_ADD_ASSIGN_R:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf += rf;
				memcpy(&mem[x - 1], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_SUB_ASSIGN_R:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf -= rf;
				memcpy(&mem[x - 1], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_MUL_ASSIGN_R:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf *= rf;
				memcpy(&mem[x - 1], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_DIV_ASSIGN_R:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf /= check_zero_float(rf);
				memcpy(&mem[x - 1], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;

			case IC_ASSIGN_AT_R:
			{
				r = mem[x - 2];
				mem[r] = mem[x - 2] = mem[x - 1];
				mem[r + 1] = mem[x - 1] = mem[x];
				x--;
			}
			break;
			case IC_ADD_ASSIGN_AT_R:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf += rf;
				memcpy(&mem[x++], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_SUB_ASSIGN_AT_R:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf -= rf;
				memcpy(&mem[x++], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_MUL_ASSIGN_AT_R:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf *= rf;
				memcpy(&mem[x++], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;
			case IC_DIV_ASSIGN_AT_R:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf /= check_zero_float(rf);
				memcpy(&mem[x++], &lf, sizeof(double));
				memcpy(&mem[i], &lf, sizeof(double));
			}
			break;

			case IC_ASSIGN_R_V:
			{
				r = dsp(mem[pc++], l);
				mem[r + 1] = mem[x--];
				mem[r] = mem[x--];
				memcpy(&lf, &mem[r], sizeof(double));
			}
			break;
			case IC_ADD_ASSIGN_R_V:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf += rf;
				memcpy(&mem[i], &lf, sizeof(double));
				x -= 2;
			}
			break;
			case IC_SUB_ASSIGN_R_V:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf -= rf;
				memcpy(&mem[i], &lf, sizeof(double));
				x -= 2;
			}
			break;
			case IC_MUL_ASSIGN_R_V:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf *= rf;
				memcpy(&mem[i], &lf, sizeof(double));
				x -= 2;
			}
			break;
			case IC_DIV_ASSIGN_R_V:
			{
				memcpy(&lf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				lf /= check_zero_float(rf);
				memcpy(&mem[i], &lf, sizeof(double));
				x -= 2;
			}
			break;

			case IC_ASSIGN_AT_R_V:
			{
				r = mem[x - 2];
				mem[r + 1] = mem[x--];
				mem[r] = mem[x--];
			}
			break;
			case IC_ADD_ASSIGN_AT_R_V:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf += rf;
				memcpy(&mem[i], &lf, sizeof(double));
				--x;
			}
			break;
			case IC_SUB_ASSIGN_AT_R_V:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf -= rf;
				memcpy(&mem[i], &lf, sizeof(double));
				--x;
			}
			break;
			case IC_MUL_ASSIGN_AT_R_V:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf *= rf;
				memcpy(&mem[i], &lf, sizeof(double));
				--x;
			}
			break;
			case IC_DIV_ASSIGN_AT_R_V:
			{
				memcpy(&lf, &mem[i = mem[x -= 2]], sizeof(double));
				lf /= check_zero_float(rf);
				memcpy(&mem[i], &lf, sizeof(double));
				--x;
			}
			break;

			case IC_EQ_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf == rf;
			}
			break;
			case IC_NE_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf != rf;
			}
			break;
			case IC_LT_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf < rf;
			}
			break;
			case IC_GT_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf > rf;
			}
			break;
			case IC_LE_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf <= rf;
			}
			break;
			case IC_GE_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				mem[x] = lf >= rf;
			}
			break;
			case IC_ADD_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				lf += rf;
				memcpy(&mem[x++], &lf, sizeof(double));
			}
			break;
			case IC_SUB_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				lf -= rf;
				memcpy(&mem[x++], &lf, sizeof(double));
			}
			break;
			case IC_MUL_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				lf *= rf;
				memcpy(&mem[x++], &lf, sizeof(double));
			}
			break;
			case IC_DIV_R:
			{
				memcpy(&lf, &mem[x -= 3], sizeof(double));
				lf /= check_zero_float(rf);
				memcpy(&mem[x++], &lf, sizeof(double));
			}
			break;
			case IC_POST_INC_R:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				++rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_POST_DEC_R:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				--rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_INC_R:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				++rf;
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_DEC_R:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				--rf;
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_POST_INC_AT_R:
			{
				memcpy(&rf, &mem[i = mem[x]], sizeof(double));
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				++rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_POST_DEC_AT_R:
			{
				memcpy(&rf, &mem[i = mem[x]], sizeof(double));
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				--rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_INC_AT_R:
			{
				memcpy(&rf, &mem[i = mem[x]], sizeof(double));
				++rf;
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_DEC_AT_R:
			{
				memcpy(&rf, &mem[i = mem[x]], sizeof(double));
				--rf;
				memcpy(&mem[x + 1], &rf, sizeof(double));
				x += 2;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_INC_R_V:
			case IC_POST_INC_R_V:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				++rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_DEC_R_V:
			case IC_POST_DEC_R_V:
			{
				memcpy(&rf, &mem[i = dsp(mem[pc++], l)], sizeof(double));
				--rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_INC_AT_R_V:
			case IC_POST_INC_AT_R_V:
			{
				memcpy(&rf, &mem[i = mem[x--]], sizeof(double));
				++rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;
			case IC_PRE_DEC_AT_R_V:
			case IC_POST_DEC_AT_R_V:
			{
				memcpy(&rf, &mem[i = mem[x--]], sizeof(double));
				--rf;
				memcpy(&mem[i], &rf, sizeof(double));
			}
			break;

			case IC_UNMINUS_R:
			{
				rf = -rf;
				memcpy(&mem[x - 1], &rf, sizeof(double));
			}
			break;
			case IC_NOT:
				mem[x] = ~mem[x];
				break;
			case IC_LOG_NOT:
				mem[x] = !mem[x];
				break;

			default:
				runtimeerr(wrong_kop, mem[pc - 1], numTh);
		}
	}

	if (numTh)
	{
		free((int *)pcPnt);
	}

	return NULL;
}

INTERPRETER_EXPORTED void import(const char *path)
{
	char firstline[100];
	int i;
	int pc;

	input = fopen(path, "r");

	if (!input)
	{
		printf(" %s not found\n", path);
		return;
	}

	/* Check shebang, get back to start if there are none */
	if (fgets(firstline, sizeof(firstline), input) != firstline || strncmp(firstline, "#!", 2) != 0)
	{
		fseek(input, 0, SEEK_SET);
	}

	fscanf(input, "%i %i %i %i %i %i %i\n", &pc, &funcnum, &id, &rp, &md, &maxdisplg, &wasmain);

	for (i = 0; i < pc; i++)
	{
		fscanf(input, "%i ", &mem[i]);
	}
	for (i = 0; i < funcnum; i++)
	{
		fscanf(input, "%i ", &functions[i]);
	}
	for (i = 0; i < id; i++)
	{
		fscanf(input, "%i ", &identab[i]);
	}
	for (i = 0; i < rp; i++)
	{
		fscanf(input, "%i ", &reprtab[i]);
	}
	for (i = 0; i < md; i++)
	{
		fscanf(input, "%i ", &modetab[i]);
	}

	fclose(input);

	threads[0] = pc;
	mem[pc] = g = pc + 2; // это l
	mem[g] = mem[g + 1] = 0;
	mem[pc + 1] = g + maxdisplg + 5; // это x
	pc = 4;

	mem[g + 2] = mem[g + 4] = 1; // для ROWING mem [g+3] for int, mem[g+5],mem[g+6] for double

#ifdef __APPLE__
	sem_unlink(sem_print);
	sempr = sem_open(sem_print, O_CREAT, S_IRUSR | S_IWUSR, 1);
#else
	sem_init(&sempr, 0, 1);
#endif
	t_init();
	interpreter(&pc); // номер нити главной программы 0
	printf("\n");
	t_destroy();
}
