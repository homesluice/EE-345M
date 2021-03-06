// ************** eFile.c *****************************
// Middle-level routines to implement a solid-state disk 
// Dustin Replogle and Katy Loeffler  3/21/2011


//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating

//#define MAX_NUM_FILES 10
#include <string.h>
#include "efile.h"            /* FatFs declarations */
#include "edisk.h"        /* Include file for user provided disk functions */
#include "drivers/ff.h"
#include "inc/hw_memmap.h"

//FIL * Files[10];
FIL CurFile;
FIL * Fp;	//Global file pointer, only one file can be open at a time
static FATFS FileSystem;
static FATFS * FsPtr;
unsigned char Buff[512];
int WriteToFile = 0;


#define GPIO_B0 (*((volatile unsigned long *)(0x40005004)))
#define GPIO_B1 (*((volatile unsigned long *)(0x40005008)))
#define GPIO_B2 (*((volatile unsigned long *)(0x40005010)))
#define GPIO_B3 (*((volatile unsigned long *)(0x40005020)))


DSTATUS eFile_MeasureWriteCapability(void)
{
  BYTE testBuffer[512];
  DSTATUS result;
  unsigned int i;
  
  for(i = 0; i < 512; i++)
  {
    testBuffer[i] = 0xFF;
  }

  result = eDisk_Init(0);
  if(result)
  {
    return result;
  }
  for(i = 1; i < 101; i++)
  {
  	GPIO_B0 = 0x01;
    result = eDisk_WriteBlock(testBuffer, i);
	GPIO_B0 = 0x00;
  }
  return result;
}

DSTATUS eFile_MeasureReadCapability(void)
{
  BYTE testBuffer[512];
  DSTATUS result;
  unsigned int i;
  
  for(i = 0; i < 512; i++)
  {
    testBuffer[i] = 0xFF;
  }

  result = eDisk_Init(0);
  if(result)
  {
    return result;
  }
  for(i = 1; i < 101; i++)
  {
  	GPIO_B0 = 0x01;
    result = eDisk_ReadBlock(testBuffer, i);
	GPIO_B0 = 0x00;
  }
  return result;
}
 


int eFile_Init(void) // initialize file system
{
  const char *path = 0; 
  FRESULT res;
  FsPtr = &FileSystem;
  res = f_mount(0, FsPtr);
  if(res) return 1;
  return 0; 
}
//---------- eFile_Format-----------------
// Erase all Files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void) // erase disk, add format
{  
  FRESULT res;
  DSTATUS result;
  BYTE ALLOC_SIZE = 16;  
  int i; 
  unsigned short block;

  for(i = 0; i<512; i++)
  {
    Buff[i] = 0;
  }
  for(block = 0; block < 0xFF; block++){
//    GPIO_PF3 = 0x08;     // PF3 high for 100 block writes
    eDisk_WriteBlock(Buff,block); // save to disk
//    GPIO_PF3 = 0x00;

  }
  res = f_mkfs(0, 0, ALLOC_SIZE);
  if(res) return 1;
  res = f_mount(0, FsPtr);   // assign initialized FS object to FS pointer on drive 0
  if(res) return 1; 
  return 0;   
}
//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[])  // create new file, make it empty 
{
  FRESULT res;
  Fp = &CurFile;
  res = f_open(Fp, name, FA_CREATE_NEW); 
  if(res) return 1;
  return 0;
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[])      // open a file for writing 
{
  FRESULT res;
  Fp = &CurFile;
  res = f_open(Fp, name, FA_WRITE);     //params: empty Fp, path ptr, mode
  if(res) return 1;
  return 0;	
}
//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data)  
{
  const void * dat = &data;
  WORD * bytesWritten;
  FRESULT res;

  res = f_write (Fp, dat, 1, bytesWritten);

  if(res) return 1;
  return 0;
}
//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void) 
{
  FRESULT res = f_mount(0, NULL);   // unmount initialized fs
  if(res)
  {
    return 1; 
  }
  return 0; 						
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) // close the file for writing
{
  FRESULT res;			/* Open or create a file */
  char data = '\0';
  const void * dat = &data;
  WORD * bytesWritten;
  res = f_write (Fp, dat, 1, bytesWritten);
  res = f_close(Fp);
  Fp = NULL;
  if(res)
  {
    return 1; 
  }
  return 0; 
}
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[])      // open a file for reading 
{
  FRESULT res;
  DIR directory;
  DIR * DirObj = &directory;
  Fp = &CurFile;
  res = f_open(Fp, name, FA_READ);     //params: empty Fp, path ptr, mode
  if(res) return 1;
  return 0;	
} 

int eFile_ResetFP(void)
{
  Fp->fptr = 0;
  return 0;
}  
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)

int eFile_ReadNext( char *pt)       // get next byte 
{
  FRESULT res;
  WORD numBytesRead = 0;
  WORD *numBytesPt = &numBytesRead;
  WORD numBytesToRead = 1;
  res = f_read(Fp, pt, numBytesToRead, numBytesPt);			/* Read data from a file */
  if(res) return 1;
  if(numBytesRead == numBytesToRead)
  {
     return 0;
  }
  else
  {
     return 1;
  }
   return 0;  ///!!
}                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) // close the file for writing
{
  FRESULT res = f_close(Fp);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
}
//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void)   
{
	FRESULT res;
  char string[30];
  DIR foundDir;
	DIR *directory = &foundDir;
  FILINFO foundFil;
  FILINFO * filinfo = &foundFil;
	res = f_opendir(directory, "");
  if(res) return 1;
	for(;;)
  {
      //
      // Read an entry from the directory.
      //
      res = f_readdir(directory, filinfo);
  
      //
      // Check for error and return if there is a problem.
      //
      if(res != FR_OK)
      {
          return(res);
      }

      //
      // If the file name is blank, then this is the end of the
      // listing.
      //
      if(!filinfo->fname[0])
      {
          break;
      }
      sprintf(string, "\t %i \r\n", filinfo->fsize);
      OSuart_OutString(UART0_BASE, filinfo->fname);
      OSuart_OutString(UART0_BASE, string);
  }
  res = f_readdir(directory, filinfo);
  if(res) return 1;
	return 0;
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(char name[])  // remove this file 
{
  FRESULT res = f_unlink(name);   /* Delete an existing file or directory */
  if(res)
  {
    return 1; 
  }
  return 0;  						
}
//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name)
{
  WriteToFile = 1;
  if(eFile_WOpen(name)){diskError("eFile_WOpen",0); return 1;}
  return 0;  
}
//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void)
{
  WriteToFile = 0;
  return eFile_WClose(); 
}
