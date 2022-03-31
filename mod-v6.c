/* Project 2 - Part 1
*  March 31, 2022
* 
* Part 1 contributions:
* Basma Mahamid : get_free_block,  main function
* 
* Adham Kassem: initfs
* Dhruv Thoutireddy:  add_free_block
*
* How to run:
* when execture the use can give two command
* initfs file_name n1 n2  :  will initialie filesystem that will be stored in file_name with n1-block size and n2 blocks devoted to inodes
* q                       : quit the program
*/




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BLOCK_SIZE 1024 
#define INODE_SIZE 64
#define DIR_SIZE 32




#define FREE_ARRAY_SIZE 200 // free and inode array size
#define MAX_NFREE 199



#define INODE_ALLOC 0x8000 //A = 1 : I-node is allocated
#define INODE_FREE 0x0000  // A = 0: I-node is free
#define DIREC_FILE 0x4000 // BC = 10: directory file









typedef struct {
    unsigned int isize; // 4 byte
    unsigned int fsize;
    unsigned int nfree;
    unsigned int free[FREE_ARRAY_SIZE];
    unsigned short flock;
    unsigned short ilock;
    unsigned short fmod;
    unsigned int time;
} super_type;

typedef struct {
    unsigned short flags;
    unsigned short nlinks;
    unsigned int uid;
    unsigned int gid;
    unsigned int size0;
    unsigned int size1;
    unsigned int addr[9];
    unsigned int actime;
    unsigned int modtime;
} inode_type;


typedef struct {
    unsigned int inode;
    char filename[28];
} dir_type;  //32 Bytes long

inode_type root;

super_type super;

 

int fd;
char pwd[100];
int curINodeNumber;
char fileSystemPath[100];
int total_inodes_count;

 
void allocateBlocks(void);
void createRootDirectory();


 

 

void initfs(const char* path, int total_blocks, int total_inode_blocks)
{

    // First open or create  filesystem
    fd = open(path, O_RDWR);

    if (fd == -1) {//file is not in system

        fd = open(path, O_CREAT | O_RDWR, 0600);// create file with R&W
        printf("File created and opened\n");
    }
    else {
        printf("File opened\n");//file found

        //read in super
        lseek(fd, BLOCK_SIZE, SEEK_SET);
        read(fd, &super, BLOCK_SIZE);

    }

    int x;
    super.isize = total_inode_blocks; // total number of blocks for inode
    super.fsize = total_blocks;// total blocks for system
    //test

    // defining variables of the super  ??? Ask prof
    super.nfree = 0;
    super.flock = 0;
    super.ilock = 0;
    super.fmod = 0;
    super.time = time(NULL);

    //set all blocks as free
    allocateBlocks();

    // writing the super block
    lseek(fd, BLOCK_SIZE, SEEK_SET);
    write(fd, &super, BLOCK_SIZE);


    // create root directory for the first inode
    createRootDirectory();

    // find number of inodes in system
    int num_inodes = (BLOCK_SIZE / INODE_SIZE) * super.isize;
    int totalBytes;
    // set other inodes to free
    totalBytes = (2 * BLOCK_SIZE) + INODE_SIZE; //start from INODE 2  .  after blocks and super block
    lseek(fd, totalBytes, SEEK_SET);

    for (x = 2; x <= num_inodes; x++) {
        inode_type nodeX;
        nodeX.flags = INODE_FREE; // set inodes to free
        write(fd, &nodeX, INODE_SIZE);//write inode to block
    }
    
    return;
}

void quit(void) {
    if (super.fsize > 0 && super.isize > 0) {
        super.time = time(NULL);//update super block access time
        lseek(fd, BLOCK_SIZE, SEEK_SET);
        write(fd, &super, BLOCK_SIZE);
    }

    exit(0); //exit system
}


int add_free_block(int bNumber)
{
    
    super.free[super.nfree] = bNumber;                  // updating the free array with the new block
    super.nfree++;
    lseek(fd, BLOCK_SIZE, SEEK_SET);//find superblock
    write(fd, &super, BLOCK_SIZE);//update nfree in superblock
}


int get_free_block(void) {
    if (super.nfree > 0) {
        super.nfree -= 1;
         lseek(fd, BLOCK_SIZE, SEEK_SET);//find super
         write(fd, &super, BLOCK_SIZE);//update nfree in super
         return super.free[super.nfree];//get free block from free array
    }
    else {
        return -1;
    }
}




void createRootDirectory()
{
    inode_type rootDir;
    dir_type direc[16];
    int freeBlock;
    int totalBytes;
    unsigned long long writeBytes;
    int i;
    freeBlock = get_free_block();
    if (freeBlock == -1) {// System was full
        printf("Error: No Free Blocks Available\n");
        return;
    }

    direc[0].inode = 1;//1st inode = root directory
    strcpy(direc[0].filename, ".");

    direc[1].inode = 1;//2nd node = parent
    strcpy(direc[1].filename, "..");



    for (i = 2; i < 16; i++) {//set rest of nodes to free
        direc[i].inode = 0;
    }


    // inode struct
    rootDir.flags = (INODE_ALLOC | DIREC_FILE); // I-node allocated + directory file
    rootDir.nlinks = 1;
    rootDir.uid = 0;
    rootDir.gid = 0;
    rootDir.addr[0] = freeBlock;
    rootDir.actime = time(NULL);
    rootDir.modtime = time(NULL);

    // directory size
    writeBytes = DIR_SIZE * 2;
    rootDir.size0 = (int)((writeBytes & 0xFFFFFFFF00000000) >> 32); // high 64 bit
    rootDir.size1 = (int)(writeBytes & 0x00000000FFFFFFFF); // low 64 bit

    //write inode to inode block
    totalBytes = (2 * BLOCK_SIZE);
    lseek(fd, totalBytes, SEEK_SET);
    write(fd, &rootDir, INODE_SIZE);

    //write root directory to data block
    totalBytes = (freeBlock * BLOCK_SIZE);
    lseek(fd, totalBytes, SEEK_SET);
    write(fd, direc, BLOCK_SIZE);

}



void allocateBlocks(void) {
    unsigned int blockIdx = super.isize + 2;//isize + super + Block0
    int unallocatedBlocks = super.fsize - blockIdx;//total data blocks
    unsigned int i;

    for (i = 0; i < unallocatedBlocks && i < FREE_ARRAY_SIZE; i++) {//write to free array
        super.free[i] = blockIdx;
        blockIdx += 1;
    }
    super.nfree = unallocatedBlocks;

    return;
}



void ProcessCommand(char* command) {

    char* token;
    char* args[5] = { NULL };  // spliced arguments
    // command
    token = strtok(command, " ");
    args[0] = token;
    char* fs_path;

    if (args[0] != NULL) {
        if ((strcmp(args[0], "initfs") == 0)) {
            fs_path = strtok(NULL, " ");


            if (access(fs_path, X_OK) != -1)
            {
                printf("filesystem already exists. \n");
                printf("same file system will be used\n");
                return;
            }


            args[1] = strtok(NULL, " ");// file system size in # blocks
            args[2] = strtok(NULL, "\n");// # nodes for i-nodes

            if ((args[1] != NULL) && (args[2] != NULL)) {
                char arg1[10];
                char arg2[10];
                int i;
                strcpy(arg1, args[1]);
                strcpy(arg2, args[2]);
                int arg1Len = strlen(arg1);
                for (i = 0; i < arg1Len; i++) { //checking all numeric inputs
                    if (isdigit(arg1[i]) == 0) {//input is not numeric
                        printf("Input is not numeric\n");
                        return;
                    }
                }
                int arg2Len = strlen(arg2);
                for (i = 0; i < arg2Len; i++) { //checking all numeric inputs
                    if (isdigit(arg2[i]) == 0) {//input is not numeric
                        printf("Input is not numeric\n");
                        return;
                    }
                }

                int n1 = atoi(args[1]);
                int n2 = atoi(args[2]);
                if (n1 <= 0 || n2 <= 0) {// no blocks or inode blocks to initialize
                    printf("Invalid size\n");
                    return;
                }

                if ((n1 < (n2 + 2))) {//number of blocks system size < inode blocks
                    printf("Invalid size\n");
                    return;
                }

                initfs(fs_path,n1, n2);
            }
            else {
                printf("Invalid Command\n");
            }

        }
        else if ((strcmp(args[0], "q\n") == 0)) {
            quit();
        }
        else {
            printf("Invalid Command\n");
        }

       
    }
    else {
        printf("Invalid Command\n");
    }

    return;
}


int main(void) {
    char command[512];
    fd = -1;
    curINodeNumber = 1; // start at the root directory;

    while (1) {
        printf("Enter command:");
        fgets(command, sizeof(command), stdin); //get command from console
        ProcessCommand(command);//find modv6 command

    }

    return 0;

}
