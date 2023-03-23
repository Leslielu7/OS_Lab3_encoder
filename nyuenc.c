
//
//  nyuenc.c
//  nyuenc

//  Created by Leslie Lu on 3/18/23.


#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>

int main(int argc, const char * argv[]){
    
    if(argc>=2){
        //open the file in command line
        ssize_t byte;
        char  c[100] ;
        char prev ;
        unsigned int count=0;
        
        for(int i=1; i<argc; i++){
            int fd_in;
            
            if((fd_in = open(argv[i],O_RDONLY, 0777))== -1){
                printf("Error open file: %s\n",argv[1]);
                return 1;
            }
        
        
            //encode the text in the file
            while((byte = read(fd_in, c, sizeof(char)))!=0){

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
                write(STDOUT_FILENO,&count,1);
             
            }
            
            close(fd_in);
        }
    }
    return 0;
}

