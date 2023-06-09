//
//  nyuenc.c
//  nyuenc

//  Created by Leslie Lu on 3/18/23.

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <pthread.h>

#define CHUNK_SIZE 4096
#define MAX_QUEUE_SIZE 4096

size_t size = 0;
int total_num_chunk = 0;
int crt_file_num =-1;

int int_argc = 0;

typedef struct Task{
    char* start;
    int task_order;
    char **task_result;
    int result_len;
    int task_len;
    int done;
}Task;


typedef struct TaskQueue{
    Task * arr;
    int front,rear; //index of the first and last tasks in the queue
    int max_queue_size;

    pthread_mutex_t mutex_Q;
    pthread_cond_t cond_Q;

}TaskQueue;

void init_TaskQueue(TaskQueue *T){
    T->arr = malloc(sizeof(Task)*MAX_QUEUE_SIZE);
    T->front = -1;
    T->rear = -1;
    T->max_queue_size = MAX_QUEUE_SIZE;
}

int is_empty(TaskQueue *T){
    return T->front == -1;//if empty, return true
}

int is_full(TaskQueue *T){
    return (T->rear)+1 % T->max_queue_size == T->front ;//if full, return true
}

void enqueue(TaskQueue *T, Task *t){
    if(is_full(T)){
        printf("TaskQueue is full\n");
        return;
    }
    if(is_empty(T)){
        T->front = 0;
        T->rear = 0;
    }
    else{
        T->rear = (T->rear+1) % T->max_queue_size;
    }

    *(T->arr + T->rear) = *t;
    
}

int dequeue(TaskQueue *T){//return the index of the dequeued item
    if(is_empty(T)){
        printf("TaskQueue is empty\n");
        return -1;
    }
    int index = T->front;
    if(T->front==T->rear){//only one item
        T->front = -1;
        T->rear = -1;
    }else{
        T->front = (T->front+1) % T->max_queue_size;
    }
    return index;
}


TaskQueue taskQueue ;

int num_tasks = 0;

pthread_mutex_t mutex_total_num_chunk;

Task taskResult [262145];
int total_task_order = 0;
int num_encoded_tasks = 0;

pthread_mutex_t mutex_R;
pthread_cond_t cond_R;

int task_waited_to_be_done = 0;

void collect_result();
void encode(Task *task);// pointer to task_order, task_result
void nonthread_encode(int argc, char* argv[]);

void submit_task(Task *task){
    //enqueue
    pthread_mutex_lock(&taskQueue.mutex_Q);
//    taskQueue[num_tasks] = *task;
    while (num_tasks == MAX_QUEUE_SIZE) {
        pthread_cond_wait(&taskQueue.cond_Q,&taskQueue.mutex_Q);
    }
    enqueue(&taskQueue,task);
    num_tasks++;
    pthread_cond_signal(&taskQueue.cond_Q);
    pthread_mutex_unlock(&taskQueue.mutex_Q);
}

void submit_task_result(Task *task){
    pthread_mutex_lock(&mutex_R);
    //enqueue
//    insert_Task(&taskResult,task,task->task_order);
    taskResult[task->task_order] = *task;
    num_encoded_tasks++;
   //signal mainthread
    pthread_cond_signal(&cond_R);
    pthread_mutex_unlock(&mutex_R);
}


void* thread_start(){//encode chunks, input chunk data(args)
    while(1){
    
        Task task;

        pthread_mutex_lock(&taskQueue.mutex_Q);
        while (num_tasks == 0) {
            
            pthread_cond_wait( &taskQueue.cond_Q, &taskQueue.mutex_Q);
            
            pthread_mutex_lock(&mutex_total_num_chunk);
                if(total_num_chunk<=0 && crt_file_num == int_argc-3-1){
                            pthread_mutex_unlock( &taskQueue.mutex_Q );
                            pthread_mutex_unlock(&mutex_total_num_chunk);
                            return NULL;
                        }
            pthread_mutex_unlock(&mutex_total_num_chunk);
        }
 
        task = *((&taskQueue)->arr+dequeue(&taskQueue));
        num_tasks--;
        pthread_cond_signal(&taskQueue.cond_Q);
        pthread_mutex_unlock(&taskQueue.mutex_Q);
        
        encode(&task);
        submit_task_result(&task);

        pthread_mutex_lock(&taskQueue.mutex_Q);
        pthread_mutex_lock(&mutex_total_num_chunk);
        if(total_num_chunk<=0 && crt_file_num == int_argc-3-1){
            pthread_cond_broadcast(&taskQueue.cond_Q);
            pthread_mutex_unlock(&mutex_total_num_chunk);
            pthread_mutex_unlock(&taskQueue.mutex_Q);
            return NULL;
        }
        pthread_mutex_unlock(&mutex_total_num_chunk);
        pthread_mutex_unlock(&taskQueue.mutex_Q);
    
    }
    return NULL;
}


void read_files(char **files, int num_files );

int main(int argc, char * argv[]){

    if(argc<2){
        perror("no input files");
    }
    
    //deciding num of threads
    int num_threads = 0;
    
    int option;
    while((option = getopt(argc, argv, "j"))!=-1){
        switch(option){
            case'j' :
                num_threads = atoi(argv[2]);
        }
    }
    //creating threads
    
        //thread ids
        pthread_t th_ids[num_threads];
    
        pthread_mutex_init(&taskQueue.mutex_Q, NULL);
        pthread_cond_init(&taskQueue.cond_Q, NULL);
    
    pthread_mutex_init(&mutex_R, NULL);
    pthread_cond_init(&cond_R, NULL);
    
        pthread_mutex_init(&mutex_total_num_chunk, NULL);
//    pthread_cond_init(&cond_total_num_chunk, NULL);

        if(!num_threads){//if num_threads is 0, don't create thread
            
            nonthread_encode(argc, argv);
            return 0;
        }
        else{//create threads
            init_TaskQueue(&taskQueue);
//            init_TaskQueue(&taskResult);
            
            int_argc = argc;
            for(int i=0; i<num_threads; i++){
                
                
                if(pthread_create(&th_ids[i], NULL, &thread_start//,(void*)fd_out
                                  ,NULL) != 0){
                    perror("pthread create failed");
                };
            }
            read_files(argv+3, argc-3);
            
            pthread_mutex_lock(&mutex_R);
            while (num_encoded_tasks == 0) {
                pthread_cond_wait(&cond_R, &mutex_R);
            }
            pthread_mutex_unlock(&mutex_R);
            
            collect_result();
            
            //stop pthreads
            for(int i=0; i<num_threads; i++){
                if(pthread_join(th_ids[i], NULL) != 0){
                    perror("pthread join failed");
                }
            }
            
            pthread_mutex_destroy(&taskQueue.mutex_Q);
            pthread_cond_destroy(&taskQueue.cond_Q);
            
            pthread_mutex_destroy(&mutex_R);
            pthread_cond_destroy(&cond_R);
           
            pthread_mutex_destroy(&mutex_total_num_chunk);

        }

    free((&taskQueue)->arr);
    
    for(int i=0;i<total_task_order;i++){
        free(*(taskResult[i].task_result));
        free((taskResult[i].task_result));
    }
    return 0;
}


void collect_result(){
    char ch,prev_ch='\0';
    unsigned  cnt=0,prev_cnt=0;
    int same = 0;
    for(int i=0;i<total_task_order;i++){
       
        pthread_mutex_lock(&mutex_R);
        task_waited_to_be_done = i;
        while(!taskResult[i].done){
            pthread_cond_wait(&cond_R, &mutex_R);
        }
        pthread_mutex_unlock(&mutex_R);
            
            for( int j =0; j<taskResult[i].result_len; j++){

                if(!(j%2)){
                    ch = *(*(taskResult[i].task_result)+j);

                    if(prev_ch!=ch && !(i==0&&j==0)){
                        write(STDOUT_FILENO,&prev_ch,1);
                        write(STDOUT_FILENO,&prev_cnt,1);

                        prev_ch = ch;
                   
                        same = 0;
                        
                    }else{
                        prev_ch = ch;
                        same = 1;
                       
                    }
                    
                }
                else{
                    cnt = *(*(taskResult[i].task_result)+j);
                    if(same){prev_cnt += cnt;}
                    else {prev_cnt = cnt;}
                }
            }
    }
    
    write(STDOUT_FILENO,&prev_ch,1);
    write(STDOUT_FILENO,&prev_cnt,1);
}

void encode(Task * task){
    
    char prev='\0';
    char ch;
    unsigned  count = 0;

    int remain = task->task_len;//CHUNK_SIZE
  
    
    *(task->task_result) = malloc((task->task_len)*2);
    
    
    int i=0;
    while(remain>0){
        if(remain ==1){
            ch = *(task->start);
         } else{
            ch = (*(task->start)++);
            }

            if(prev!=ch &&  count>0){

                *((*(task->task_result))+(i++)) = prev;
                *((*(task->task_result))+(i++)) = count;

            count = 0;
            (task->result_len)+=2;
        
        }
        prev=ch;
        count++;
        remain--;
        
      
        //print out last char and its count
        if(!remain){
            *((*(task->task_result))+(i++)) = prev;
            *((*(task->task_result))+(i++)) = count;
    
            (task->result_len )+=2;
        }
    }
 
    pthread_mutex_lock(&mutex_total_num_chunk);
    total_num_chunk--;
    pthread_mutex_unlock(&mutex_total_num_chunk);
    task->done = 1;
    
    pthread_mutex_lock(&mutex_R);
    if(task_waited_to_be_done==task->task_order){
        pthread_cond_signal(&cond_R);
    }
    pthread_mutex_unlock(&mutex_R);
    
    return;
}

void read_files(char **files, int num_files ){
    FILE * fd_in;
    
    struct stat sb;
    void * addr;

    for(int i=0; i<num_files; i++){
      //  printf("file:%s\n",files[i]);
       
        //open file
        if((fd_in = fopen(files[i],"rb"))==NULL){
            printf("Error open file: %s\n",files[i]);
            return;
        }
        // Get file size
        if (fstat(fileno(fd_in), &sb) == -1){
            printf("fstat failed, i:%d\n",i);
        }
        
        if( (addr = mmap(NULL, sb.st_size , PROT_READ, MAP_PRIVATE, fileno(fd_in), 0))== MAP_FAILED){
                printf("mmap failed, i:%d\n",i);
                exit(1);
            }else{
                
                size += (int)sb.st_size;

            }

        int num_chunk = sb.st_size % CHUNK_SIZE > 0 ? (sb.st_size / CHUNK_SIZE) +1 : sb.st_size / CHUNK_SIZE;//num_chunk in this file

    
        for (int i = 0; i < num_chunk; i++) {
            Task t = {
                
                .start = (char*)addr+i*CHUNK_SIZE,
                
                .task_order = i+total_task_order,
                .task_result = malloc(sizeof(char*)),//char** task_result
                .result_len = 0,
                .task_len = i== num_chunk-1 ? sb.st_size-CHUNK_SIZE*(num_chunk-1) :CHUNK_SIZE,
                .done = 0
            };
             submit_task(&t);
         }
        pthread_mutex_lock(&mutex_total_num_chunk);
        total_num_chunk += num_chunk;
        crt_file_num++;
        pthread_mutex_unlock(&mutex_total_num_chunk);
        total_task_order += num_chunk;
        if((fd_in)==NULL){printf("NULL\n");}
        if((fclose(fd_in))!=0){
            printf("fclose:%d\n",errno);
        }
        
    }
  
}


void nonthread_encode(int argc, char* argv[]){
    ssize_t byte;
             char  c[100] ;
             char prev='\0';
            unsigned count=0;

             for(int i=1; i<argc; i++){
                 int fd_in;

                 if((fd_in = open(argv[i],O_RDONLY, 0777))== -1){
                     printf("Error open file: %s\n",argv[1]);
                     return;
                 }

                 //encode the text in the file
                 while((byte = read(fd_in, c, sizeof(char)))!=0){
                    // if(!count){ c[0] = byte;prev=byte;}
                     if ( c[0]!= prev){
                         if(prev || count>0){
                             write(STDOUT_FILENO,&prev,sizeof(char));
                             write(STDOUT_FILENO,&count,1);

                             count = 0;
                         }
                     }
                     prev=c[0];
                     count ++;
                 };

                 // print last element if this is the last file in command line
                 if(i==argc-1 && byte == 0 && count !=0){
                     write(STDOUT_FILENO,&prev,sizeof(char));
                     //count = count +48;
                     write(STDOUT_FILENO,&count,1);

                 }

                 close(fd_in);
             }
    return;
}
