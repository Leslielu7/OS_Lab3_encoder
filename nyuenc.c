
//
//  nyuenc.c
//  nyuenc

//  Created by Leslie Lu on 3/18/23.


#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#include <pthread.h>

#define THREAD_NUM 4
#define CHUNK_SIZE 4096

//void encode(int argc, const char * argv[]);
void encode(char ** chunks);
//void* thread_start(void* args){//encode chunks, input chunk data(args)
//    printf("args:%c",*args);
//    //encode(chunk data);
//    //encode(argc, argv);
//
//    return NULL;
//}
void* read_files(const char **files, int num_files, size_t *size, char **chunks
                    ){
    int fd_in;
    char * addr;
    struct stat sb;
    
    for(int i=0; i<num_files; i++){
        printf("file:%s\n",files[i]);
        //open files
        if((fd_in = open(files[i],O_RDONLY, 0777))== -1){
            printf("Error open file: %s\n",files[i]);
        }
        // Get file size
        if (fstat(fd_in, &sb) == -1){
            //handle_error();
            printf("fstat failed, i:%d\n",i);
        }
        
            addr = mmap(NULL, sb.st_size , PROT_READ, MAP_PRIVATE, fd_in, 0);
            if (addr == MAP_FAILED){
                // handle_error();
                printf("mmap failed, i:%d\n",i);
            }else{
                memcpy(chunks, &addr, sizeof(addr));
            }

            *size += sb.st_size;
            chunks++;
            close(fd_in);
    }
   
    (chunks)--;
    return chunks;
}

int main(int argc, const char * argv[]){
   
    if(argc<2){
        perror("no input files");
    }
    
    char ** chunks = malloc(sizeof(char*)) ;
    *chunks = malloc(1000);
   
    size_t size;
    char *  data = read_files(argv+1, argc-1, &size, chunks);

    printf("outside chunks:%s\n", (*chunks));
    chunks++;
    printf("outside chunks:%s\n", (*chunks));
    //printf("outside data:%s\n", data);
 
    free(--chunks);
    munmap(data,size);
    
    return 0;
}

//void encode(char ** start_chunk){
////    size_t byte;
////    char  c[100] ;
//
//    char * ptr = *start_chunk;
//    char prev;
//    int position = 0;
//    unsigned int count = 0;
//    while(position<4096){
////        if(*ptr != prev){
////            if(prev || count>0){
////                write(STDOUT_FILENO,&prev,sizeof(char));
////                write(STDOUT_FILENO,&count,1);
////
////                count = 0;
////            }
////        }
//
//        prev = *ptr;
//        printf("%c\n",prev);
//        ptr ++;
//        count ++;
//        position ++;
//    }
//    // print the last element if this is the last file in command line
//   //            if(i==argc-1 && byte == 0 && count !=0){
//   //                write(STDOUT_FILENO,&prev,sizeof(char));
//   //                write(STDOUT_FILENO,&count,1);
//   //
//   //            }
//
//}


//void encode(int argc, const char * argv[]){
//    if(argc>=2){
//        //open the file in command line
//        ssize_t byte;
//        char  c[100] ;
//        char prev ;
//        unsigned int count=0;
//
//        for(int i=1; i<argc; i++){
//            int fd_in;
//
//            if((fd_in = open(argv[i],O_RDONLY, 0777))== -1){
//                printf("Error open file: %s\n",argv[1]);
//                //return 1;
//            }
//
//
//            //encode the text in the file
//            while((byte = read(fd_in, c, sizeof(char)))!=0){
//
//                if ( c[0]!= prev){
//                    if(prev || count>0){
//                        write(STDOUT_FILENO,&prev,sizeof(char));
//                        write(STDOUT_FILENO,&count,1);
//
//                        count = 0;
//
//                    }
//                }
//                prev=c[0];
//                count ++;
//
//            };
//
//            // print the last element if this is the last file in command line
//            if(i==argc-1 && byte == 0 && count !=0){
//                write(STDOUT_FILENO,&prev,sizeof(char));
//                write(STDOUT_FILENO,&count,1);
//
//            }
//
//            close(fd_in);
//        }
//    }
//}
