#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
//#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define INODE_SIZE 64
#define MAX_FILE_SIZE 4194304 // 4GB of file size
#define FREE_ARRAY_SIZE 248 // free and inode array size

typedef struct {
    unsigned int isize; // 4 byte
    unsigned int fsize;
    unsigned int nfree;
    unsigned short free[FREE_ARRAY_SIZE];
    unsigned int ninode;
    unsigned short inode[FREE_ARRAY_SIZE];
    unsigned short flock;
    unsigned short ilock;
    unsigned short fmod;
    unsigned int time[2];
} super_block;

typedef struct {
    unsigned short flags; // 2 bytes
    char nlinks;  // 1 byte
    char uid;
    char gid;
    unsigned int size; // 32bits  2^32 = 4GB filesize
    unsigned short addr[22]; // to make total size = 64 byte inode size
    unsigned int actime;
    unsigned int modtime;
} Inode;

typedef struct
{
    unsigned short inode;
    char filename[14];
}dEntry;

super_block super;
int fd;
char pwd[100];
int curINodeNumber;
char fileSystemPath[100];
int total_inodes_count;


int open_fs(char *file_name){
    fd = open(file_name, O_RDWR | O_CREAT, 0600);

    if(fd == -1){
        return -1;
    }
    else{
        return 1;
    }
}

// Function to write inode
void inode_writer(int inum, inode_type inode){

   lseek(fd,2*BLOCK_SIZE+(inum-1)*INODE_SIZE,SEEK_SET); 
    write(fd,&inode,sizeof(inode));
}

// Function to read inodes
inode_type inode_reader(int inum, inode_type inode){
   lseek(fd,2*BLOCK_SIZE+(inum-1)*INODE_SIZE,SEEK_SET); 
    read(fd, &inode, sizeof(inode));
    return inode;
}

// Function to write inode number after filling some fields
void fill_an_inode_and_write(int inum){
    inode_type root;
    int i;

    root.flags |= 1 << 15; //Root is allocated
    root.flags |= 1 <<14; //It is a directory
    root.actime = time(NULL);
    root.modtime = time(NULL);

    root.size0 = 0;
    root.size1 = 2 * sizeof(dir_type);
    root.addr[0]=100; //assuming that blocks 2 to 99 are for i-nodes; 100 is the first data block that can hold root's directory contents
	for (i=1;i<9;i++) root.addr[i]=-1;//all other addr elements are null so setto -1
    inode_writer(inum, root);
}

void writeToBlock(int blockNumber, void * buffer, int nbytes){
        lseek(fd,BLOCK_SIZE * blockNumber, SEEK_SET);
        write(fd,buffer,nbytes);
}

void createRootDirectory(){
        int blockNumber = getFreeBlock();
        dEntry directory[2];
        directory[0].inode = 0;
        strcpy(directory[0].filename,".");

        directory[1].inode = 0;
        strcpy(directory[1].filename,"..");

        writeToBlock(blockNumber, directory, 2*sizeof(dEntry));

        Inode root;
        root.flags = 1<<14 | 1<<15; // setting 14th and 15th bit to 1, 15: allocated and 14: directory
        root.nlinks = 1;
        root.uid = 0;
        root.gid = 0;
        root.size = 2*sizeof(dEntry);
        root.addr[0] = blockNumber;
        root.actime = time(NULL);
        root.modtime = time(NULL);

        writeInode(0,root);
        curINodeNumber = 0;
        strcpy(pwd,"/");
}

void initfs(char* file_name, int n1, int n2)
{
        printf("\n filesystem intialization started \n");
        total_inodes_count = n2;
        char emptyBlock[BLOCK_SIZE] = {0};
        int no_of_bytes,i,blockNumber,iNumber;

        //init isize (Number of blocks for inode
        if(((n2*INODE_SIZE)%BLOCK_SIZE) == 0) // 300*64 % 1024
                super.isize = (n2*INODE_SIZE)/BLOCK_SIZE;
        else
                super.isize = (n2*INODE_SIZE)/BLOCK_SIZE+1;

        //init fsize
        super.fsize = n1;

        //create file for File System
        if((fd = open(file_name,O_RDWR|O_CREAT,0600))== -1)
        {
                printf("\n file opening error [%s]\n",strerror(errno));
                return;
        }
        strcpy(fileSystemPath,file_name);

        writeToBlock(n1-1,emptyBlock,BLOCK_SIZE); // writing empty block to last block

        // add all blocks to the free array
        super.nfree = 0;
        for (blockNumber= 1+super.isize; blockNumber< n1; blockNumber++)
                addFreeBlock(blockNumber);

        // add free Inodes to inode array
        super.ninode = 0;
        for (iNumber=1; iNumber < n2 ; iNumber++)
                addFreeInode(iNumber);


        super.flock = 'f';
        super.ilock = 'i';
        super.fmod = 'f';
        super.time[0] = 0;
        super.time[1] = 0;

        //write Super Block
        writeToBlock(0,&super,BLOCK_SIZE);

        //allocate empty space for i-nodes
        for (i=1; i <= super.isize; i++)
                writeToBlock(i,emptyBlock,BLOCK_SIZE);

        createRootDirectory();
}

void quit()
{
        close(fd);
        exit(0);
}


// The main function
int main(){

    inode_type inode1;
    open_fs("Test_fs.txt");
    fill_an_inode_and_write(1);
    inode1=inode_reader(1,inode1);
    printf("Value of inode1's addr[0] is %d\n",inode1.addr[0]);
    printf("Value of inode1's addr[1] is %d\n",inode1.addr[1]);
}
