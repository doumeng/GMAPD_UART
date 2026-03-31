#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include "appExample.h"

int exitApp(int a)
{
    std::cout << "app exit " <<std::endl;

    img::imgStopInput();

    _exit(0);
    return 0;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, exitApp);
    signal(SIGSEGV, exitApp);
    int opt;
    bool flag = false;

    while ((opt = getopt(argc, argv, "i")) != -1) {
        switch (opt) {
            case 'i':
                flag = true;
                break;
            default:
                printf("invalid parameter\n");
                break;
        }
    }

    helloWorld();

    //电源
    boardPwCtrlInit();

    if(flag){
        img::imgInit();
        createGetImgThd();
    }

    cli_cmd_start();
    cli_cmd_end();
    
    while (1){
        sleep(10);
    }

    return 0;
}