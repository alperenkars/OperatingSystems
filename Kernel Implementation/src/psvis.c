#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {

    int is_module_loaded = system("lsmod | grep psvis_module > /dev/null");
    if (is_module_loaded != 0) {
        
// psvis_module not loaded, ask for sudo password 
        printf("psvis_module is not loaded. Please enter sudo password to load the module.\n");
        fflush(stdout);
        system("sudo insmod psvis_module.ko");
    } else {
        printf("psvis_module is already loaded.\n");
    }

    
    atexit(unload_module);
    
    // visualization
    
    return 0;
}

void unload_module() {
    // Unload the psvis_module 
    printf("Unloading psvis_module.\n");
    fflush(stdout);
    system("sudo rmmod psvis_module");
}

