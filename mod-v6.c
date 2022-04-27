/* Project 2 - Part 2
*
* Part 2 contributions:
* Basma Mahamid : cpout, processcommand, IsSystemInitialized, updateInodeEntry, getFreeInode.
* Adham Kassem: cpin, getInode, and helped editing in other areas.
* Dhruv Thoutireddy: rm, openfileS, and helped editing in other areas.
*
* How to run:
* when executed the user can input one of two commands:
*           initfs file_name n1 n2  :  initialize filesystem that will be stored in file_name with n1-block size and n2 blocks devoted to inodes
*   cpin externalfile internalfile  :  creates a new file called internalfile in the v6 file system and fill the contents of the newly created file with the contents of the externalfile
* cpout sourcefile destinationfile  :  creates destinationfile and make the destinationfile's contents equal to sourcefile
*                       rm v6-file  :  deletes the file v6_file from the v6 file system and removes all the data blocks of the file, frees the i-node and removes the directory entry
*                                q  :  quit the program
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




#define INODE_ALLOC 0x8000 // A = 1 : INode is being used
#define SETUID_EXEC 0x0400//F = 1
#define SETGID_EXEC 0x0200//G = 1
#define INODE_FREE 0x0000  // A = 0: I-node is free
#define DIREC_FILE 0x4000 // BC = 10: directory file
#define PLAIN_FILE 0x0000 // BC = 00: plain file
#define BLOCK_FILE 0x6000 // BC = 11: block file
#define MEDIUM_FILE 0x0800// DE = 01
#define LONG_FILE 0x1000// DE = 10
#define SUPERLONG_FILE 0x1800//DE = 11
#define CHAR_FILE 0x2000 // BC = 01: char special file
#define SMALL_FILE 0x0000// DE = 00



#define INODE_SIZE 64
#define MAX_NFREE 250
#define BLOCK_SIZE 1024
#define FREE_ARRAY_SIZE 251 // free and inode array size
#define DIR_SIZE 32





/*filesystem structure/ superblock reserved for system*/
typedef struct {
    unsigned int isize; //num of inode blocks
    unsigned int fsize; //filesystem size
    unsigned int nfree; //number of free blocks
    unsigned int free[FREE_ARRAY_SIZE];
    unsigned short flock;
    unsigned short ilock;
    unsigned short fmod;
    unsigned int time;
} super_type;

/*defining structure and attributes for inode*/
typedef struct {
    unsigned short flags;
    unsigned short nlinks;
    unsigned int uid;
    unsigned int gid;
    //both size0 and size1 combined in the case of storing very large files (>4GB)
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


//global variables
super_type SuperBlock;
int FileHandler;
int currentInode;
char currentFilepath[200];
inode_type rootDir;

//Prototypes
int openfileS(char *file);
void allocateBlocks(void);
void createRootDirectory();
void initfs(const char* path, int nblocks, int nBlockforInode);
int IsSystemInitialized(void);
void addFreeBlock(unsigned int block);
int cpin(char* externalFile, char* internalFile);
void cpout(char* sourceFile, char* destinationFile);
void updateInodeEntry(int addBytes, int inodeNum, inode_type newNode);

/*initfs() initializes the file system
it passes: the name of the file used to store the filesystem data,
the size of the file system in blocks,
and the num of i-node blocks*/
void initfs(const char* path, int nblocks, int nBlockforInode)
{
    //open the file with Read/Write permissions
    FileHandler = open(path, O_RDWR);

    //checks if the file is not in system then create file
    if (FileHandler == -1) {

        FileHandler = open(path, O_CREAT | O_RDWR, 0600);
        printf("File created and opened\n");
    }
    else {

        //if the file exists in the system it will open 
        //and read the superblock from the file
        printf("File opened\n");

        lseek(FileHandler, BLOCK_SIZE, SEEK_SET);
        read(FileHandler, &SuperBlock, BLOCK_SIZE);
    }

    int x;
    SuperBlock.isize = nBlockforInode; //num. of blocks reserved for the i-node
    SuperBlock.fsize = nblocks; //total num. of blocks in the system
    SuperBlock.nfree = 0;
    SuperBlock.flock = 0;
    SuperBlock.ilock = 0;
    SuperBlock.fmod = 0;
    SuperBlock.time = time(NULL);

    //initializing all blocks to be free blocks
    allocateBlocks();

    //writing the SuperBlock block
    lseek(FileHandler, BLOCK_SIZE, SEEK_SET);
    write(FileHandler, &SuperBlock, BLOCK_SIZE);

    //creating root directory for the first i-node only
    createRootDirectory();

    //finding total num. of i-nodes in system
    int num_inodes = (BLOCK_SIZE / INODE_SIZE) * SuperBlock.isize;
    int totalBytes;

    //calculating the offset where the second node needs to be written
    totalBytes = (2 * BLOCK_SIZE) + INODE_SIZE;
    lseek(FileHandler, totalBytes, SEEK_SET);

    //write all data about inodes to filesystem after the superblock block (starting from 3rd block)
    for (x = 2; x <= num_inodes; x++) {
        inode_type nodeX;
        nodeX.flags = INODE_FREE; //setting inodes to free
        write(FileHandler, &nodeX, INODE_SIZE);
    }

    return;
}

//returns inode_type variable based on inode index
inode_type getInode(unsigned int nINode) {

    inode_type inode;
    // first calculates the where the inode is present

    int totalBytes = (2 * BLOCK_SIZE) + (INODE_SIZE * (nINode - 1));
    lseek(FileHandler, totalBytes, SEEK_SET);
    read(FileHandler, &inode, INODE_SIZE);                  // reads the inode values into inode variable
    return inode;
}

/*deletes the file v6_file from the v6 file system
and removes all the data blocks of the file, frees the i-node
and removes the directory entry*/
void rm(char* filename) {

    int i, j;
    unsigned long long directory_size;
    dir_type directory[200];

    //check if file is opened
    int check = IsSystemInitialized();
    if (check == -1)  //issue with file system
    {
        return;
    }
    // get the current inode
    inode_type currentDirectoryInode = getInode(currentInode);

    // total size of the current directory
    directory_size = (currentDirectoryInode.size0 << 32) | (currentDirectoryInode.size1);
    int inode_blocks = (directory_size / BLOCK_SIZE) + (directory_size % BLOCK_SIZE != 0);
    int x = 0;
    dir_type temp[DIR_SIZE];
    int removeFlag = 0; // file has been removed
    for (x = 0; x < inode_blocks; x++) {
        int block = currentDirectoryInode.addr[x];                                       // returns the first block stored in the addr of the inode
        lseek(FileHandler, (block * BLOCK_SIZE), SEEK_SET);      // finding the address of the block of the inode
        read(FileHandler, directory, BLOCK_SIZE);              // read the contents of the blocks of addr into buffer data structure


         // checks through each directory entry
        for (i = 0; i < DIR_SIZE; i++) {


            if ((strcmp(filename, directory[i].filename) == 0) && (directory[i].inode > 0)) {      // if the file to be deleted has been found

                inode_type fileInode = getInode(directory[i].inode);
                unsigned int* fileBlocks = fileInode.addr;          // storing the block numbers of the inode of the filename to be copied

                if (fileInode.flags == (INODE_ALLOC | PLAIN_FILE)) {                     // if the inode is allocated & regular file
                   // calculates the size of file
                    unsigned long long fileInode_size = (fileInode.size0 << 32) | (fileInode.size1);

                    // iterates through every block in the addr and add the blocks to the free array
                    for (j = 0; j < (fileInode_size / BLOCK_SIZE); j++) {
                        int block = fileInode.addr[j];

                        // add the block number to the free array
                        addFreeBlock(block);
                    }
                    if ((fileInode_size % BLOCK_SIZE) > 0) {
                        int block = fileInode.addr[j];

                        // add the block number to the free array
                        addFreeBlock(block);
                    }
                    inode_type file_inode = getInode(directory[i].inode);  //get inode of file 
                    int file_inode_number = directory[i].inode;

                    directory[i].inode = 0;                // this directory entry no longer exists so making inode 0
                    strcpy(directory[i].filename, "");		// remove file entry 


                    file_inode.flags = INODE_FREE;
                    file_inode.modtime = time(NULL);                      // updating the modifying time
                    file_inode.actime = time(NULL);                       // updating the accessing time
                    file_inode.nlinks = 0;
                    directory_size -= DIR_SIZE;

                    currentDirectoryInode.size0 = (int)((directory_size & 0xFFFFFFFF00000000) >> 32);        // high 64 bit
                    currentDirectoryInode.size1 = (int)(directory_size & 0x00000000FFFFFFFF);                 // low 64 bit

                  // writing the updated directory into the block at it's correct position
                    lseek(FileHandler, (BLOCK_SIZE * currentDirectoryInode.addr[x]), SEEK_SET);
                    write(FileHandler, directory, BLOCK_SIZE);

                    // writing the updated file inode into the block at it's correct position
                    int inode_block = (file_inode_number * INODE_SIZE) / BLOCK_SIZE;
                    int offset = ((file_inode_number - 1) * INODE_SIZE) % BLOCK_SIZE;
                    lseek(FileHandler, (inode_block * BLOCK_SIZE) + offset, SEEK_SET);
                    write(FileHandler, &file_inode, INODE_SIZE);
                    removeFlag = 1;
                    printf("File removed\n");
                    return;
                }
                else {
                    printf("Error: File not found\n");
                    return;
                }

            }

        }

    }

    if (removeFlag == 0) {
        printf("Error: File not found\n");
        return;
    }

}


//quits program when user enters 'q' as a command input (prompted in main())

void quit(void) {
    if (SuperBlock.fsize > 0 && SuperBlock.isize > 0) {
        SuperBlock.time = time(NULL);
        lseek(FileHandler, BLOCK_SIZE, SEEK_SET);
        write(FileHandler, &SuperBlock, BLOCK_SIZE);
    }

    exit(0);
}

// Check for system and file initialization
int IsSystemInitialized(void) {

    // Check if a file is opened

    if (FileHandler < 2)
    {
        printf("Error: File is not opened \n");
        return -1;
    }
    // Now check if file was initialized
    if (SuperBlock.fsize < 1 || SuperBlock.isize < 1) {
        printf("Error: File not initialized\n");
        return -1;
    }
    return 1;
}

/*add_free_block() takes the address of the block that is no longer being used
and adds it to the free array of free blocks to be used later*/

int add_free_block(int bNumber)
{
 
    if (SuperBlock.nfree == MAX_NFREE) {                   // if the free array is full, store the free blocks in a new block
        lseek(FileHandler, BLOCK_SIZE * bNumber, SEEK_SET);
        write(FileHandler, SuperBlock.nfree, sizeof(int));
        write(FileHandler, SuperBlock.free, (FREE_ARRAY_SIZE) * sizeof(int));
        SuperBlock.nfree = 0;
    }
    SuperBlock.free[SuperBlock.nfree] = bNumber;                  // updating the free array with the new block
    SuperBlock.nfree++;
    lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
    write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock

}

/*if there are any free blocks found (>0) the the address of a free data block
from the free array will be returned
in case of an error/if there are no free blocks left, -1 will be returned*/

int get_free_block(void) {

    SuperBlock.nfree -= 1;//get next free block from free array

    if (SuperBlock.nfree > 0) {
        if (SuperBlock.free[SuperBlock.nfree] == 0) { //system is full, return error

            SuperBlock.nfree++;
            lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
            write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
            return -1;
        }
        else {
            lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
            write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
            return SuperBlock.free[SuperBlock.nfree];//get free block from free array

        }
    }
    else {//get new set of free blocks for free array
        int newBlock = SuperBlock.free[0];
        int totalBytes = (newBlock * BLOCK_SIZE);

        lseek(FileHandler, totalBytes, SEEK_SET);//find block from free array
        read(FileHandler, &SuperBlock.nfree, sizeof(int)); //read in nfree into super block
        read(FileHandler, SuperBlock.free, (FREE_ARRAY_SIZE) * sizeof(int)); // read 251 bytes to free array

        //write free array to SuperBlock
        lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
        write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
        return SuperBlock.free[SuperBlock.nfree];// get free block from free array
    }

}

//creates root directory with the two entries . and ..
void createRootDirectory()
{
    //inode_type rootDir; ##########
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

    direc[1].inode = 1;//2nd inode = parent
    strcpy(direc[1].filename, "..");

    //sets all other nodes to free
    for (i = 2; i < 16; i++) {
        direc[i].inode = 0;
    }


    // inode struct for root directory
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

    //writing inodes to inode block
    totalBytes = (2 * BLOCK_SIZE);
    lseek(FileHandler, totalBytes, SEEK_SET);
    write(FileHandler, &rootDir, INODE_SIZE);

    //write root directory to data block
    totalBytes = (freeBlock * BLOCK_SIZE);
    lseek(FileHandler, totalBytes, SEEK_SET);
    write(FileHandler, direc, BLOCK_SIZE);

}


/*this function allocates the free blocks
and stores the index of the free blocks and updates size array of free blocks in superblock*/

void allocateBlocks(void) {
    unsigned int blockIdx = SuperBlock.isize + 2;//isize + SuperBlock + Block0
    int unallocatedBlocks = SuperBlock.fsize - blockIdx;//total data blocks
    unsigned int nextFree[252] = { 0 };//next free array
    unsigned int nextnFree = 0;// nfree value
    unsigned int storageBlockIdx = 0;
    unsigned int nextStorageBlockIdx = 0;
    unsigned int totalBytes = 0;
    unsigned int i;


    if (unallocatedBlocks < FREE_ARRAY_SIZE) {//number of blocks < free array size
        nextnFree = unallocatedBlocks;
        for (i = 0; i < unallocatedBlocks; i++) {//write to free array
            SuperBlock.free[i] = blockIdx;
            blockIdx += 1;
        }
        SuperBlock.nfree = nextnFree;

        storageBlockIdx = SuperBlock.free[0];


        //write last storage block
        nextFree[2] = storageBlockIdx;
        nextFree[1] = 0;
        nextFree[0] = 0;
        nextnFree = 2;
        totalBytes = (storageBlockIdx * BLOCK_SIZE); //calculate block number
        lseek(FileHandler, totalBytes, SEEK_SET);
        write(FileHandler, &nextnFree, sizeof(int));//store nfree value in indirect block
        write(FileHandler, nextFree, sizeof(int) * FREE_ARRAY_SIZE);
        return;
    }
    else {//number of blocks >= free array size
        nextnFree = MAX_NFREE;
        //write to free array to superblock
        for (i = 0; i < FREE_ARRAY_SIZE; i++) {
            SuperBlock.free[i] = blockIdx;
            blockIdx += 1;
        }

        SuperBlock.nfree = nextnFree;//set nfree
        unallocatedBlocks = unallocatedBlocks - FREE_ARRAY_SIZE;//find number of unallocated blocks
        storageBlockIdx = SuperBlock.free[0];//next storage block value

        while (blockIdx < SuperBlock.fsize) {


            if (unallocatedBlocks > FREE_ARRAY_SIZE) {//free blocks > 251
                nextFree[MAX_NFREE] = storageBlockIdx;

                for (i = 0; i < MAX_NFREE; i++) {//create array of free blocks
                    if (i == 0) {
                        nextFree[i] = blockIdx;
                        nextStorageBlockIdx = blockIdx;//store next storage block value
                    }
                    else {
                        nextFree[i] = blockIdx;
                    }
                    blockIdx += 1;
                }
                nextnFree = MAX_NFREE;// set new nfree value

                //write to storage block
                totalBytes = (storageBlockIdx * BLOCK_SIZE); //calculate block number
                lseek(FileHandler, totalBytes, SEEK_SET);//find block
                write(FileHandler, &nextnFree, sizeof(int));//store nfree value in indirect block
                write(FileHandler, nextFree, (FREE_ARRAY_SIZE) * sizeof(int)); //store free blocks in indirect block

                //update unallocated blocks
                storageBlockIdx = nextStorageBlockIdx;//keep track of next storage block
                unallocatedBlocks = unallocatedBlocks - MAX_NFREE; //find number of blocks to allocate
            }
            else {
                nextFree[unallocatedBlocks] = storageBlockIdx;//free blocks < 251
                for (i = 0; i < unallocatedBlocks; i++) {//create array of free blocks
                    if (i == 0) {
                        nextStorageBlockIdx = blockIdx;//store next storage block value
                    }

                    nextFree[i] = blockIdx;
                    blockIdx += 1;
                }
                nextnFree = unallocatedBlocks;// set new nfree value
                //write to storage block
                totalBytes = (storageBlockIdx * BLOCK_SIZE); //calculate block number
                lseek(FileHandler, totalBytes, SEEK_SET);
                write(FileHandler, &nextnFree, sizeof(int));//store nfree value in indirect block
                write(FileHandler, nextFree, (unallocatedBlocks + 1) * sizeof(int)); //store free blocks in indirect block + indirect block #

                //last storage block
                storageBlockIdx = nextStorageBlockIdx;
            }

        }

        //write last storage block
        nextFree[2] = storageBlockIdx;
        nextFree[1] = 0;
        nextFree[0] = 0;
        nextnFree = 2;
        totalBytes = (storageBlockIdx * BLOCK_SIZE); //calculate block number
        lseek(FileHandler, totalBytes, SEEK_SET);
        write(FileHandler, &nextnFree, sizeof(int));//store nfree value in indirect block
        write(FileHandler, nextFree, sizeof(int) * FREE_ARRAY_SIZE);

        return;
    }

}


/*processes command given by user (currently only initfs and quit are implemented)
if command inputted is not one of these two commands or initfs is not written with valid/correct number of arguments
.. error message will appear: invalid command*/

void ProcessCommand(char* command) {

    char* token;
    char* args[5] = { NULL };
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

                initfs(fs_path, n1, n2);
            }


        }
        else if ((strcmp(args[0], "cpin") == 0)) {
            args[1] = strtok(NULL, " ");// system external file
            args[2] = strtok(NULL, "\n");// file system v6 file
            if (args[2] == NULL || args[1] == NULL) {
                printf("Invalid Command\n");
            }
            else {
                int check = cpin(args[1], args[2]);
                if (check > 0) {
                    printf("File transfer success\n");
                }
                else {
                    printf("File transfer failed\n");
                }
            }
        }
        else if ((strcmp(args[0], "cpout") == 0)) {
            args[1] = strtok(NULL, " ");// system external file
            args[2] = strtok(NULL, "\n");// file system v6 file

            if (args[2] == NULL || args[1] == NULL) {
                printf("Invalid Command\n");
            }
            else {
                cpout(args[1], args[2]);
            }

        }
        else if ((strcmp(args[0], "rm") == 0)) {
            args[1] = strtok(NULL, "\n");// file system v6 file
            if (args[1] == NULL) {
                printf("Invalid Command\n");
            }
            else {
                rm(args[1]);
            }
        }
        else if ((strcmp(args[0], "q\n") == 0)) {
            quit();
        }
        else if(strcmp(args[0], "openfs") == 0){
            args[1] = strtok(NULL, "\n");
            if(args[1] == NULL){
                printf("Invalid Command\n");
            }
            else{
                if(openfileS(args[1]) == 1){
                    printf("File opened\n");
                }
                else{
                    printf("error occurred");
                }
            }
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

//returns index of free inode
int getFreeInode(void) {
    int x;
    int num_inodes = (BLOCK_SIZE / INODE_SIZE) * SuperBlock.isize;
    int totalBytes;
    inode_type freeNode;

    totalBytes = (2 * BLOCK_SIZE);	//read inode from block
    lseek(FileHandler, totalBytes, SEEK_SET);
    for (x = 1; x <= num_inodes; x++) {				//find next free inode

        read(FileHandler, &freeNode, INODE_SIZE);

        if ((freeNode.flags & INODE_ALLOC) == 0) {			//check if inode is free
            return x;										//return next free inode value
        }
    }
    return -1; // did not find free inode

}


/*if there are any free blocks found (>0) the the address of a free data block
from the free array will be returned
in case of an error/if there are no free blocks left, -1 will be returned*/
int getFreeBlock(void) {
    SuperBlock.nfree -= 1;//get next free block from free array

    if (SuperBlock.nfree > 0) {
        if (SuperBlock.free[SuperBlock.nfree] == 0) { //system is full, return error

            SuperBlock.nfree++;
            lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
            write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
            return -1;
        }
        else {
            lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
            write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
            return SuperBlock.free[SuperBlock.nfree];//get free block from free array

        }
    }
    else {//get new set of free blocks for free array
        int newBlock = SuperBlock.free[0];
        int totalBytes = (newBlock * BLOCK_SIZE);

        lseek(FileHandler, totalBytes, SEEK_SET);//find block from free array
        read(FileHandler, &SuperBlock.nfree, sizeof(int)); //read in nfree into SuperBlock block
        read(FileHandler, SuperBlock.free, (FREE_ARRAY_SIZE) * sizeof(int)); // read 251 bytes to free array

        //write free array to SuperBlock
        lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
        write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
        return SuperBlock.free[SuperBlock.nfree];// get free block from free array
    }

}



int findDirectory(char* dir_name, int parentNode) {
    // find parent folder directory
    inode_type rootNode;
    int totalBytes = (2 * BLOCK_SIZE) + ((parentNode - 1) * INODE_SIZE);
    lseek(FileHandler, totalBytes, SEEK_SET);
    read(FileHandler, &rootNode, INODE_SIZE);

    // check to see if inode is a directory file
    if ((rootNode.flags & DIREC_FILE) != DIREC_FILE) { //not a directory file
        printf("Error: File is not a directory file\n");
        return -1;
    }

    // go to size and see what is size
    unsigned long long size = ((rootNode.size0 << 32) | rootNode.size1); //find directory size

    // go to addr[]
    dir_type buffer[DIR_SIZE];
    int totalBlocks = size / BLOCK_SIZE; // total blocks in file
    int fpFound = 0; // folder Inode value

    while (totalBlocks > -1) {
        //load block to temp buffer
        totalBytes = rootNode.addr[totalBlocks] * BLOCK_SIZE;
        lseek(FileHandler, totalBytes, SEEK_SET);
        read(FileHandler, buffer, BLOCK_SIZE);

        //read through list to find directory value
        int i = 0;
        for (i = 0; i < DIR_SIZE; i++) {
            if (strcmp(buffer[i].filename, dir_name) == 0) {
                fpFound = buffer[i].inode;
                return fpFound;				//return inode if found
            }
        }
        totalBlocks--;
        // search through inodes to find directory value
    }
    return fpFound;
}

//creates destinationfile and make the destinationfile's contents equal to sourcefile
void cpout(char* sourceFile, char* destinationFile) {
    inode_type node;
    int i, j;
    unsigned int block;
    unsigned long long inode_size;
    char buffer[BLOCK_SIZE] = { 0 };                              // temperoray storage to read the contents of block into
    dir_type directory[100];

    //check source file is open
    int check = IsSystemInitialized();
    if (check == -1)  //issue with file system
    {
        return;
    }

    // external file is opened
    int destination = open(destinationFile, O_RDWR | O_CREAT, 0600);
    if (destination == -1) {
        printf("File error\n");
        return;
    }

    //check if source file is in directory
    check = findDirectory(sourceFile, currentInode);
    if (check == 0) {
        printf("Error: File is not in directory\n");
        return;
    }

    // gets the inode of the current directory
    node = getInode(currentInode);

    // total size of the current directory
    inode_size = (node.size0 << 32) | (node.size1);

    int inode_blocks = (inode_size / BLOCK_SIZE) + (inode_size % BLOCK_SIZE != 0);
    int x = 0;
    int writeFlag = 0;		// flag - file written to system
    dir_type temp[DIR_SIZE];
    for (i = 0; i < inode_blocks; i++) {
        block = node.addr[i];                                       // returns the first block stored in the addr of the inode
        lseek(FileHandler, (block * BLOCK_SIZE), SEEK_SET);      // finding the address of the block of the inode
        read(FileHandler, temp, BLOCK_SIZE);              // read the contents of the blocks of addr into buffer data structure
        for (j = 0; j < DIR_SIZE; j++) {
            directory[x] = temp[j];
            x++;
        }
    }
    // checks through each directory entry
    for (i = 0; i < (inode_size / DIR_SIZE); i++) {

        if ((strcmp(sourceFile, directory[i].filename) == 0) && (directory[i].inode > 0)) {    // if the file to be copied to external file has been found

            inode_type fileInode = getInode(directory[i].inode);

            if (fileInode.flags == (INODE_ALLOC | PLAIN_FILE)) {                   // if the inode is allocated & regular file
           // calculates the size of file
                unsigned long long fileInode_size = (fileInode.size0 << 32) | (fileInode.size1);

                // iterates through every block in the addr and stores the content in buffer
                lseek(destination, 0, SEEK_SET);

                for (j = 0; j < (fileInode_size / BLOCK_SIZE); j++) {

                    block = fileInode.addr[j];
                    lseek(FileHandler, (block * BLOCK_SIZE), SEEK_SET);
                    read(FileHandler, buffer, BLOCK_SIZE);
                    write(destination, buffer, BLOCK_SIZE);
                }

                // stores the offset of the last block into the buffer

                if (fileInode_size % BLOCK_SIZE != 0) {
                    block = fileInode.addr[j];
                    lseek(FileHandler, (block * BLOCK_SIZE), SEEK_SET);
                    read(FileHandler, buffer, fileInode_size % BLOCK_SIZE);
                    write(destination, buffer, fileInode_size % BLOCK_SIZE);
                }
                node.actime = time(NULL);

                // writing the updated inode into the block at it's correct position
                int totalBytes = (2 * BLOCK_SIZE) + (INODE_SIZE * (currentInode - 1));
                lseek(FileHandler, totalBytes, SEEK_SET);
                read(FileHandler, &node, INODE_SIZE);
                writeFlag = 1;						//file written to system
                printf("File written to system\n");
            }
            else {                                               // if the inode is unallocated, there is no file present
                printf("Error: File not found\n");
                return;
            }

        }
    }
    if (writeFlag == 0) {
        printf("Error: File Transfer Failed\n");
        return;
    }


}


/*takes the address of the block that is no longer being used
and adds it to the free array of free blocks to be used later*/
void addFreeBlock(unsigned int block) {
    if (SuperBlock.nfree == MAX_NFREE) {                   // if the free array is full, store the free blocks in a new block
        lseek(FileHandler, BLOCK_SIZE * block, SEEK_SET);
        write(FileHandler, SuperBlock.nfree, sizeof(int));
        write(FileHandler, SuperBlock.free, (FREE_ARRAY_SIZE) * sizeof(int));
        SuperBlock.nfree = 0;
    }
    SuperBlock.free[SuperBlock.nfree] = block;                  // updating the free array with the new block
    SuperBlock.nfree++;
    lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
    write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
}

//adds new file directory entry and updates inodes
int addNewFileDirectoryEntry(int parentInodeNum, dir_type newDir) {
    inode_type parentNode;

    //get file inode struct
    int totalBytes = (2 * BLOCK_SIZE) + ((parentInodeNum - 1) * INODE_SIZE);
    lseek(FileHandler, totalBytes, SEEK_SET);
    read(FileHandler, &parentNode, INODE_SIZE);

    int check = findDirectory(newDir.filename, parentInodeNum); 	//check if file is in directory
    if (check > 0) {
        printf("Error: File is already in directory\n");
        return -1;
    }
    if (check == -1) {
        printf("Error: Not accessing directory file\n");
        return -1;
    }


    // go to size and see what is size
    unsigned long long size = ((parentNode.size0 << 32) | parentNode.size1); //find directory size

    // go to addr[] and get blocks
    dir_type buffer[DIR_SIZE];
    int totalBlocks = size / BLOCK_SIZE; // total blocks in file
    int dirNum = -1;
    int writeflag = 0;		//flag - file entry added
    while (totalBlocks > -1 && writeflag == 0) {

        //load block to temp buffer
        totalBytes = parentNode.addr[totalBlocks] * BLOCK_SIZE;
        lseek(FileHandler, totalBytes, SEEK_SET);
        read(FileHandler, buffer, BLOCK_SIZE);

        // search through inodes to find directory value
        int i = 0;
        for (i = 0; i < DIR_SIZE; i++) {
            if (buffer[i].inode == 0) {			//find empty directory location and put in new entry
                dirNum = i;					//location in directory

                totalBytes = (parentNode.addr[totalBlocks] * BLOCK_SIZE) + (dirNum * DIR_SIZE);
                lseek(FileHandler, totalBytes, SEEK_SET);
                write(FileHandler, &newDir, DIR_SIZE);
                writeflag = 1;
                break;
            }
        }
        if (dirNum < 0) {//go to next block to find empty

            totalBlocks--;
        }
        else { break; }
    }

    if (writeflag == 0) {
        //if directory is full find a new block
        //need to add new block to inode
        int freeBlock = getFreeBlock();
        if (freeBlock == -1) {// System was full
            printf("Error: No Free Blocks Available. Directory not created. \n");
            return -1;
        }
        int nextBlock = size / BLOCK_SIZE;				//find how many blocks to write to 
        if (nextBlock < 7) {							// for small file
            parentNode.addr[nextBlock + 1] = freeBlock;
            totalBytes = parentNode.addr[freeBlock] * BLOCK_SIZE;
            lseek(FileHandler, totalBytes, SEEK_SET);
            write(FileHandler, &newDir, DIR_SIZE);


            totalBlocks = totalBlocks + 1;


            writeflag = 1;
        }
        else { //need to convert to large file

        }

    }

    // update inode entry
    updateInodeEntry(DIR_SIZE, parentInodeNum, parentNode);

    return 1;			//update success

}

//opens file system
int openfileS(char *file){
    FileHandler = open(file,2);
    lseek(FileHandler, BLOCK_SIZE, SEEK_SET);
    read(FileHandler, & SuperBlock,sizeof(SuperBlock));
    lseek(FileHandler, 2 * BLOCK_SIZE, SEEK_SET);
    read(FileHandler, & rootDir,sizeof(rootDir));
    return 1;
}

//updates inode index entry with new inode data
void updateInodeEntry(int addBytes, int inodeNum, inode_type newNode) {

    unsigned long long size = ((newNode.size0 << 32) | newNode.size1); //find directory size
    int totalBytes = (2 * BLOCK_SIZE) + ((inodeNum - 1) * INODE_SIZE);
    unsigned long long writeBytes = size + addBytes;			//get total new bytes of file
    newNode.size0 = (int)((writeBytes & 0xFFFFFFFF00000000) >> 32); // high 64 bit
    newNode.size1 = (int)(writeBytes & 0x00000000FFFFFFFF); // low 64 bit
    newNode.actime = time(NULL);
    newNode.modtime = time(NULL);
    lseek(FileHandler, totalBytes, SEEK_SET);
    write(FileHandler, &newNode, INODE_SIZE);

}


/*creates a new file called internalfile in the v6 file system
and fill the contents of the newly created file with the contents of the externalfile*/
int cpin(char* externalFile, char* internalFile) {

    //get length of external file
    unsigned long long file_size;
    inode_type inode;
    int offset;
    unsigned long long inode_size;


    //check if file system is opened & valid
    int check = IsSystemInitialized();
    if (check == -1)  //issue with file system
    {
        return -1;
    }

    //check external file is open
    int source_fd = open(externalFile, O_RDONLY);
    if (source_fd == -1) {
        printf("Error: File not found\n");
        return -1;
    }

    file_size = lseek(source_fd, 0, SEEK_END); // seek to end of file
    lseek(source_fd, 0, SEEK_SET); // seek back to beginning of file


    //get blocks
    int num_of_blocks = file_size / 1024 + (file_size % 1024 != 0);
    int nfree_reset = SuperBlock.nfree;
    int free_reset[251];
    memcpy(free_reset, SuperBlock.free, sizeof(free_reset));



    //get data blocks
    int blocks[num_of_blocks];
    int i = 0;
    for (i = 0; i < num_of_blocks; i++) {
        int free_block = getFreeBlock();
        if (free_block == -1) {
            printf("Error: Not enough system memory\n");

            //reset superblock
            SuperBlock.nfree = nfree_reset;
            memcpy(SuperBlock.free, free_reset, sizeof(free_reset));

            //rewrite superblock
            lseek(FileHandler, BLOCK_SIZE, SEEK_SET);//find superblock
            write(FileHandler, &SuperBlock, BLOCK_SIZE);//update nfree in superblock
            return -1;
        }
        blocks[i] = free_block;
    }

    //write to data blocks
    for (i = 0; i < num_of_blocks; i++) {
        int bytes = 1024;
        if (i == num_of_blocks - 1) {			//last block get the offset
            bytes = file_size - (i * 1024);
        }
        char buffer[bytes];
        offset = blocks[i] * 1024;
        lseek(FileHandler, offset, SEEK_SET);
        read(source_fd, buffer, bytes);				//get block from source file
        write(FileHandler, buffer, bytes);		// write block to v6 system

    }

    //get inode for new file
    int inode_idx = getFreeInode();

    //set fields except address
    inode.flags = (INODE_ALLOC | PLAIN_FILE); // I-node allocated + directory file
    inode.nlinks = 1;
    inode.uid = 0;
    inode.gid = 0;
    inode.size0 = (int)((file_size & 0xFFFFFFFF00000000) >> 32);
    inode.size1 = (int)(file_size & 0x00000000FFFFFFFF);
    inode.actime = time(NULL);
    inode.modtime = time(NULL);
    //if small file:
    if (num_of_blocks < 9) {
        for (i = 0; i < num_of_blocks; i++)
            inode.addr[i] = blocks[i];
    }
    //if large file:
    else {
        for (i = 0; i < num_of_blocks; i++) {
            //todo
        }
    }
    updateInodeEntry(0, inode_idx, inode);

    //create new directory entry
    dir_type dir_entry;
    dir_entry.inode = inode_idx;
    strcpy(dir_entry.filename, internalFile);
    check = addNewFileDirectoryEntry(currentInode, dir_entry);
    if (check < 0) {//error occurred
        return -1;
    }

    return 1;
}


/*main function where command is taken from user input and sent to be processed..
the current i-node will always be initialized back to 0 when the program is ran
in order to start at the root directory*/

int main(void) {
    char command[512];
    FileHandler = -1;
    currentInode = 1;

    while (1) {
        printf("Enter command:");
        fgets(command, sizeof(command), stdin);
        ProcessCommand(command);
    }

    return 0;
}
