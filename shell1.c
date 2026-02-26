#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>

#define RED_TEXT "\033[1;31m"
#define GREEN_TEXT "\033[1;32m"
#define YELLOW_TEXT "\033[1;33m"
#define RESET_COLOR "\033[0m"

int command(char** list, char fl);
int background_task_init(char** list);


char** read_input() // функция разбивает пользовательский ввод на команды, по сути приводит его к "правильному" виду
{
                                                //пользовательский ввод записывается в raw_user_inp 
    char *raw_user_inp = (char*)calloc(sizeof(char), 8);
    if(raw_user_inp == NULL)
    {
        fprintf(stderr, RED_TEXT "ERROR: calloc error\n" RESET_COLOR); //сообщение об ошибке при выделении памяти
        return NULL;
    }
    int remained_space_count = 8;
    int k = 0;                  
    char c;                                            
    while ((c = getchar()) != '\n')             //посимвольно читаем строку со стандартного ввода
    {
        raw_user_inp[k] = c;
        k++;
        remained_space_count--;
        if (remained_space_count == 0)
        {
            raw_user_inp = (char*)realloc(raw_user_inp, k + 8);
            if(raw_user_inp == NULL)
            {
                fprintf(stderr, RED_TEXT "ERROR: realloc error\n" RESET_COLOR); //очередная ошибка выделения памяти, 
                                                                                //дальше не буду комментировать их, ибо лень :)
                return NULL;
            }
            remained_space_count += 4;
        }
    }
    if(k == 0)                  //на случай отсутствия содержательного ввода
    {
        free(raw_user_inp);
        return NULL;
    }
    raw_user_inp[k] = 0; 
    char* correct_user_inp = (char*)calloc(strlen(raw_user_inp)*2 + 1, sizeof(char));
     //для дальнейшего разбиения переделываем строку, т.к. хотим привести её к "правильному" виду
    if(correct_user_inp == NULL)
    {
        fprintf(stderr, RED_TEXT "ERROR: calloc error\n" RESET_COLOR);
        return NULL;
    }
    k = 0;
    for(int i = 0; i < strlen(raw_user_inp); i++)
    {
        if((strchr("|&>\0", raw_user_inp[i]) != NULL) && (raw_user_inp[i] == raw_user_inp[i+1]))
        {
            correct_user_inp[k] = ' ';
            correct_user_inp[k+1] = raw_user_inp[i];
            correct_user_inp[k+2] = raw_user_inp[i+1]; //просто добавляем пробелы
            correct_user_inp[k+3] = ' ';
            i++;
            k += 4;
        }
        else if(strchr("|&>\0", raw_user_inp[i]) != NULL)
        {
            correct_user_inp[k] = ' ';
            correct_user_inp[k+1] = raw_user_inp[i];
            correct_user_inp[k+2] = ' ';
            k += 3;
        }
        else
        {
            correct_user_inp[k] = raw_user_inp[i];
            k++;
        }
    }
    correct_user_inp[k] = 0;        
    free(raw_user_inp);             //больше он нам не нужен
                                    //создаём возвращаемый массив
    char** user_str = (char**)calloc(8, sizeof(char*));
    if(user_str == NULL)
    {
        fprintf(stderr, RED_TEXT "ERROR: calloc error\n" RESET_COLOR);
        free(correct_user_inp);
        return NULL;
    }
    remained_space_count = 8;
    k = 0;
    raw_user_inp = strtok(correct_user_inp, " \0"); //разбиваем строку на части
    do
    {
        user_str[k] = (char*) calloc(strlen(raw_user_inp)+1, sizeof(char));
        strcpy(user_str[k], raw_user_inp); 
        k++;
        remained_space_count--; //всё так же, как и с юзерским вводом
        if (remained_space_count == 0)
        {
            user_str = (char**)realloc(user_str, (k + 8) * sizeof(char*));
            if(user_str == NULL)
            {
                fprintf(stderr, RED_TEXT "ERROR: realloc error\n" RESET_COLOR);
                return NULL;
            }
            remained_space_count += 4;
        }
        raw_user_inp = strtok(NULL, " \0"); //продолжаем разделять строку
    }while(raw_user_inp != NULL);
    user_str[k] = raw_user_inp; //там будет NULL
    free(correct_user_inp);
    free(raw_user_inp);
    return user_str;

}



void deleteL(char** delobj)//тут вроде всё понятно, функция удаления строкового массива
{
    for(int i = 0; delobj[i] != NULL; i++)
    {
        free(delobj[i]);
    }
    free(delobj);
}



int change_Dir(char* dir) //cd XD
{
    int temp = 0;
    if (chdir(dir) != 0)
    {
        fprintf(stderr, RED_TEXT "ERROR: no such directory %s\n" RESET_COLOR, dir);
    }
    return temp;
}



//перенаправление ввода вывода (> <, >>)
int Redirection_Handler(char** redirection_list) //работаем с перенаправленным файлом
{  
    int file_d;
    int i = 0;
    while(redirection_list[i] != NULL)
    {
        if(strcmp(redirection_list[i], ">") == 0)
        {
            //fprintf(stderr, YELLOW_TEXT "check\n" RESET_COLOR);
            file_d = open(redirection_list[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0666);   //открываем/создаём файл и удаляем его содержимое
            if(file_d == -1)
            {
                fprintf(stderr, RED_TEXT "ERROR: cannot open/create1 %s\n" RESET_COLOR, redirection_list[i+1]);
                return 1;
            }
            dup2(file_d, 1);        //перенаправляем стандартный вывод
            close(file_d);
        }
        else if(strcmp(redirection_list[i], ">>") == 0)
        {
            file_d = open(redirection_list[i+1], O_WRONLY | O_APPEND | O_CREAT, 0666);  //открываем/создаём файл и НЕ удаляем его содержимое
            if(file_d == -1)
            {
                fprintf(stderr, RED_TEXT "ERROR: cannot open/create2 %s\n" RESET_COLOR, redirection_list[i+1]);
                return 1;
            }
            
            lseek(file_d, SEEK_END, 0);
            dup2(file_d, 1);
            lseek(1, SEEK_END, 0);
            close(file_d);
        }
        else if(strcmp(redirection_list[i], "<") == 0)
        {
            file_d = open(redirection_list[i+1], O_RDONLY);     //открываем на чтение
            if(file_d == -1)
            {
                fprintf(stderr, RED_TEXT "ERROR: cannot open/create3 %s\n" RESET_COLOR, redirection_list[i+1]);
                return 1;
            }
            dup2(file_d, 0);
            close(file_d);
        }
        i += 2;
    }
    return 0;
}

int check_condititons(char** list, char fl)
{
    int ex_status = 0;
    int condition = 0;
    char** curr_list2;
    int k = 0;
    int i, ok;
    while(1)
    {
        curr_list2 = (char**) calloc(256, sizeof(char*));
        if(curr_list2 == NULL)
        {
            fprintf(stderr, RED_TEXT "ERROR: calloc error\n" RESET_COLOR);
            return 1;
        }
        i = 0;
        ok = 0;
        if(strcmp(list[k], "(") == 0)   //подсчёт ( и ), чтоб их количество совпадало, и формирование команды в скобках
        {
            ok++;
            curr_list2[i] = list[k];
            i++;
            k++;
            while(1)
            {
                curr_list2[i] = list[k];
                i++;
                k++;
                if(strcmp(list[k-1], "(") == 0)
                {
                    ok++;
                }
                else if(strcmp(list[k-1], ")") == 0)
                {
                    ok--;
                }
                if(ok == 0)
                {
                    break;
                }
            }
        }
        else //формируем очередную команду, все действия аналогичны таким же в execute_commands
        {
            while((list[k] != NULL) && (strcmp(list[k], "&&") != 0) && (strcmp(list[k], "||") != 0))
            {
                curr_list2[i++] = list[k++];
            }
        }
        curr_list2[i] = NULL;
        if(ex_status == condition)              //если все норм, то выполняем команду
            ex_status = command(curr_list2, fl);               
        if(list[k] == NULL)
            break;
        condition = strcmp(list[k], "&&") != 0;
        k++;
        free(curr_list2);
    }
    free(curr_list2);
    return ex_status;
}



int execute_commands(char** list) //выполнение команд (обработанного вывода пользователя)
{
    pid_t pid;
    if((pid = fork()) == 0) //сын
    {
        int ex_status = 0;
        int bracket_balance;
        int k = 0;
        int i, j; //i для list, j для curr_list
        while(1)
        {
            char** curr_list = (char**) calloc(256, sizeof(char*));
            i = k;
            j = 0;
            bracket_balance = 0;
            if(strcmp(list[i], "(") == 0) // (*нечто*) делаем отдельно
            {
                bracket_balance++;
                curr_list[j] = list[i];
                j++;
                i++;
                while(1)
                {
                    curr_list[j] = list[i];
                    j++;
                    i++;
                    if(strcmp(list[i-1], "(") == 0)
                    {
                        bracket_balance++;
                    }
                    else if(strcmp(list[i-1], ")") == 0)
                    {
                        bracket_balance--;
                    }
                    if(bracket_balance == 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                while(((list[i] != NULL) && (strcmp(list[i], "&") != 0) && (strcmp(list[i], ";") != 0)) || (bracket_balance != 0)) //выделяем отдельные команды/скобки
                {
                    if(strcmp(list[i], "(") == 0)
                        bracket_balance++;
                    if(strcmp(list[i], ")") == 0)
                        bracket_balance--;
                    curr_list[j] = list[i];
                    j++;
                    i++;
                }
            }
            curr_list[j] = NULL; //конец списка
            if((list[i] != NULL) && (strcmp(list[i], "&") == 0))//выполнение в фоновом режиме
            {
                ex_status |= background_task_init(curr_list);
            }
            else
            {
                ex_status |= check_condititons(curr_list, 0); //выполнение
            }
            if(list[i++] != NULL)
            {
                if(list[i] == NULL)
                {
                    deleteL(curr_list);     //конец команд
                    break;
                }
                else
                {
                    k = i;                  //переход дальше
                }
            }
            else
            {
                deleteL(curr_list);
                break;
            }
            free(curr_list);
        }
        exit(ex_status);
    }
    if(pid < 0)
    {
        fprintf(stderr, RED_TEXT "ERROR: fork() ret %d\n" RESET_COLOR, pid);
        abort();
    }
    int st;
    waitpid(pid, &st, 0);
    if(WIFEXITED(st) != 0)    // возвращаем 8 младших битов
        return WEXITSTATUS(st);
    else
        return -1;
}



int command(char** list, char fl)
{
    if(strcmp(list[0], "cd") == 0)      //отлов cd
    {
        if((list[1] == NULL) || (list[2] != NULL))
        {
            fprintf(stderr, RED_TEXT "ERROR: wrong amount of arguments in cd\n" RESET_COLOR);
            return 1;
        }
        else
        {
            return change_Dir(list[1]);
        }
    }
    if(strcmp(list[0], "(") == 0)
    {
        int k;
        for(k = 0; list[k] != NULL; k++);
        list[k-1] = NULL;
        return execute_commands(list+1);
    }
    char** redirection_list = (char**) calloc(5, sizeof(char*)); //имеет вид {>, file, < file, NULL}
    if(redirection_list == NULL){
        fprintf(stderr, RED_TEXT "ERROR: calloc error occurred\n" RESET_COLOR);
        return 1;
    }
    int i = 0;
    int j = 0;
    int size_of_command_list = 0;
    while(list[size_of_command_list++] != NULL); //вычисляется длина списка
    char** pipeline_list = (char**) calloc(size_of_command_list, sizeof(char*));  //конвейер
    if(pipeline_list == NULL)
    {
        fprintf(stderr, RED_TEXT "ERROR: calloc error occurred\n" RESET_COLOR);
        deleteL(redirection_list);
        return 1;
    }
    if((strcmp("<", list[0]) == 0) || (strcmp(">", list[0]) == 0) || (strcmp(">>", list[0]) == 0))  //если перенаправление в начале
    {                                                                                               // последовательности команд
        redirection_list[j] = list[0];
        redirection_list[j+1] = list[1];
        j += 2;
        i += 2;
        if((strcmp("<", list[2]) == 0) || (strcmp(">", list[2]) == 0) || (strcmp(">>", list[2]) == 0))//если есть второе перенаправление
        {
            redirection_list[j++] = list[2];
            redirection_list[j++] = list[3];
            i+= 2;
        }
    }
    else if((size_of_command_list > 2) && ((strcmp("<", list[size_of_command_list-3]) == 0) || (strcmp(">", list[size_of_command_list-3]) == 0) || (strcmp(">>", list[size_of_command_list-3]) == 0))) //если перенаправление в конце последовательности команды
    {
        redirection_list[j] = list[size_of_command_list-3];
        redirection_list[j+1] = list[size_of_command_list-2];
        j += 2;
        size_of_command_list -= 2;
        if((size_of_command_list > 2) && ((strcmp("<", list[size_of_command_list-3]) == 0) || (strcmp(">", list[size_of_command_list-3]) == 0) || (strcmp(">>", list[size_of_command_list-3]) == 0)))
        {
            redirection_list[j] = list[size_of_command_list-3];
            redirection_list[j+1] = list[size_of_command_list-2];
            j += 2;
            size_of_command_list -= 2;
        }
    }
    redirection_list[j] = NULL; // привели к виду {>, file, < file, NULL}
    j = 0;
    int pipeline_size = 1;
    for(i = i; i<size_of_command_list-1; i++)   //считаем, сколько будет команд на конвеере
    {
        if(strcmp(list[i], "|") == 0)
            pipeline_size++;
        /*if((strcmp("<", list[i]) == 0) || (strcmp(">", list[i]) == 0) || (strcmp(">>", list[i]) == 0))
        {
            i++;
            continue;
        }*/
        pipeline_list[j] = list[i];
        //fprintf(stderr, "%s\n", pipeline_list[j]);
        j++;
    }
    pipeline_list[j] = NULL;
    char** temp_ppline = pipeline_list;
    pid_t pid_array[pipeline_size];
    int fd[2];                              //для обмена через канал
    for(i = 0; i < pipeline_size; i++)
    {
        //fprintf(stderr, "\n");
        pipe(fd);
        int prevfd;
        char** cur_command = (char**) calloc((size_of_command_list), sizeof(char*));
        if(cur_command == NULL)
        {
            fprintf(stderr, RED_TEXT "ERROR: calloc error occurred\n" RESET_COLOR);
            deleteL(redirection_list);
            free(pipeline_list);
            return 1;
        }
        if(i != 0)
            for (int hg = 0; hg < 5; hg++)
                redirection_list[hg] = NULL;
        int k = 0, kl = 0, pup = 0;
        int cnt1 = 0, cnt2 = 0, cnt3 = 0;
        while(temp_ppline[pup] != NULL)
            {//fprintf(stderr, "%s\n", temp_ppline[pup]);
            //printf("%ld\n", sizeof(temp_ppline) / sizeof(temp_ppline[0]));
            pup++;}
        //fprintf(stderr, "%s\n\n\n", temp_ppline[pup]);
        //printf("\n");
        while((temp_ppline[k] != NULL) && (strcmp(temp_ppline[k], "|") != 0)) //выделяем команду в конвеере
        {
            //fprintf(stderr, "%d\n", k);
            if((strcmp("<", temp_ppline[k]) != 0) && (strcmp(">", temp_ppline[k]) != 0) && (strcmp(">>", temp_ppline[k]) != 0))
            {
                cur_command[kl] = temp_ppline[k];
                //fprintf(stderr, "%s\n", cur_command[k]);
                kl++;
            }
            else
            {
                redirection_list[cnt2] = temp_ppline[k];
                redirection_list[cnt2+1] = temp_ppline[k+1];
                //fprintf(stderr, "%d\n", k);
                //fprintf(stderr, "%s\n", redirection_list[cnt2]);
                //fprintf(stderr, "%s\n", redirection_list[cnt2+1]);
                cnt2 += 2;
                cnt1 += 2;
                cnt3++;
                //fprintf(stderr, "%d\n", k);
                if(temp_ppline[k+2] != NULL)
                if((strcmp("<", temp_ppline[k+2]) == 0) || (strcmp(">", temp_ppline[k+2]) == 0) || (strcmp(">>", temp_ppline[k+2]) == 0))//если есть второе перенаправление
                {
                    redirection_list[cnt2++] = temp_ppline[k+2];
                    redirection_list[cnt2++] = temp_ppline[k+3];
                    cnt1+= 2;
                    cnt3++;
                }
                k++;
                //fprintf(stderr, "%d\n", k);
            }
            k++;
        }
        //fprintf(stderr, "ok\n");
        //for (int hg = 0; hg < 5; hg++)
            //fprintf(stderr, "%s\n", redirection_list[hg]);
        cur_command[k] = NULL;
        temp_ppline += k+1; //бахаем начало темп_пплайн на чледующуй команду
        //cur_command начинаем исполнять
        if((pid_array[i] = fork()) == 0) //сыночек ибо в конвеере 
        {
            //fprintf(stderr, "ok\n");
            
            if(fl == 0)
            {
                signal(SIGINT, SIG_DFL); //сигинт игнорим
            }
            if(Redirection_Handler(redirection_list) == 1) //ну если не пошло в Redirection_Handler
            {
                fprintf(stderr, RED_TEXT "ERROR: redirection error\n" RESET_COLOR);
                exit(1);
            }
            //fprintf(stderr, "ok1\n");
            if(i != 0) //перенаправление ввода со стандартного на вывод из предыдущей команды конвеера
            {
                dup2(prevfd, 0);
            }
            //fprintf(stderr, "ok2\n");
            if(i != pipeline_size-1) //кАнальные приколы чтоб он работал
            {
                dup2(fd[1], 1);
            }
            
            //fprintf(stderr, "ok3\n");
            close(fd[0]);
            close(fd[1]);
            //fprintf(stderr, "ok4\n");
            execvp(cur_command[0], cur_command); //бахаем команду на выполнение
            //fprintf(stderr, "ok5\n");
            fprintf(stderr, RED_TEXT "ERROR: cannot execute %s\n" RESET_COLOR, cur_command[cnt3]);
            exit(1);
        }
        if(pid_array[i] < 0)    //коли вилка не удалась
        {
            fprintf(stderr, RED_TEXT "ERROR: fork() ret %d\n" RESET_COLOR, pid_array[i]);
            abort();
        }
        free(cur_command);
        prevfd = fd[0];
        close(fd[1]);
    }
    //конец работы конвеера
    free(pipeline_list);
    free(redirection_list);
    for(i = 0; i < pipeline_size-1; i++)
    {
        waitpid(pid_array[i], NULL, 0); //ждём завершения каждого сынки
    }
    int pipeline_status;
    waitpid(pid_array[pipeline_size-1], &pipeline_status, 0);
    if(WIFEXITED(pipeline_status) == 0) //если у сына были проблемы и он не завершился
    {                
        if(WSTOPSIG(pipeline_status)) //возвращает номер сигнала, из-за которого сын был остановлен
            {printf("%d", WSTOPSIG(pipeline_status));
            return WSTOPSIG(pipeline_status);}
        fprintf(stderr, RED_TEXT "ERROR: pipeline executing error\n" RESET_COLOR);
        //perror("\n");
        return 1;
    }
    else
    {
        return WEXITSTATUS(pipeline_status);
    }

}





int background_task_init(char** list) //реализация фонового режима
{
    pid_t pid;
    if((pid = fork()) == 0) //сЫночка-тунеядец
    {
        if(fork() == 0) //внучок
        {
            signal(SIGINT, SIG_IGN);
            int dev_null = open("/dev/null", O_RDWR);   //перенаправление как указано в задании
            dup2(dev_null, 0);
            dup2(dev_null, 1);
            close(dev_null);
            check_condititons(list, 1);     //выполняем фоновый процесс
            //sleep(20);                      //ждём завершения на всякий
            exit(0);
        }
        if(pid < 0)
        {
            fprintf(stderr, RED_TEXT "ERROR: fork() ret %d\n" RESET_COLOR, pid);
            abort();
        }
        exit(0);
    }
    if(pid < 0)
    {
        fprintf(stderr, RED_TEXT "ERROR: fork() ret %d\n" RESET_COLOR, pid);
        abort();
    }
    int st;
    waitpid(pid, &st, 0);
    if(WIFEXITED(st) == 0)
        return 1;
    else
        return WEXITSTATUS(st);
}




int main()
{
    int ex_status;
    system("clear");
    char** user_input;
    while(1)
    {
        printf(GREEN_TEXT "$ " RESET_COLOR);
        user_input = read_input();
        if(user_input != NULL)
        {
            if(strcmp(user_input[0], "exit") == 0)
            {
                deleteL(user_input);
                break;
            }
            ex_status = execute_commands(user_input); //отправляем на выполнениеFexecute
            if(ex_status && (ex_status != SIGINT))
            {
                if(ex_status == -1)
                    fprintf(stderr, "ERROR: invalid command construction\n");
                else
                    fprintf(stderr, "ERROR: command exit status is %d\n", ex_status);
            }
            deleteL(user_input);  // освобождаем память
        }
    }
    return 0;
}
