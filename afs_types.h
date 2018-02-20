#if !defined(_AFS_TYPES_H_)
#define _AFS_TYPES_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#include <string>
#include <list>
#include <vector>

#define NCYLS   203                     //!< Number of cylinders
#define NHEADS  2                       //!< Number of heads
#define NSECS   12                      //!< Number of sectors per track
#define NPAGES  (NCYLS*NHEADS*NSECS)    //!< Number of pages on one disk image
#define PAGESZ  (256*2)                 //!< Number of bytes in one page (data is actually words)
#define FNLEN   40                      //!< Maximum length of a file name

typedef uint16_t word;                  //!< Storage type of Alto file system (big endian words)
typedef uint8_t byte;                   //!< Well known type...
typedef ssize_t page_t;                 //!< Page number type

/**
 * @brief Endian test union
 * Initialize little.e = 1;
 * If little.l == 1 the machine is little endian
 * If little.l == 0 the machine is big endian
 */
typedef union {
    uint16_t e;
    uint8_t  lh[2];
} endian_t;

/**
 * @brief Structure of the disk image pages.
 * The pagenum field is not physically stored on the Diablo disks,
 * but an extension of the software which created the *.dsk files.
 *
 * The other fields are on each disk, separated by gaps and sync patterns.
 */
typedef struct afs_page_s
{
    word        pagenum;                //!< page number (think LBA)
    word        header[2];              //!< Header words
    word        label[8];               //!< Label words
    word        data[256];              //!< Data words
} afs_page_t;

/**
 * @brief Structure of a disk label.
 * This is the internal structure of afs_page_t::label
 */
typedef struct
{
    word        next_rda;               //!< Next raw disk address
    word        prev_rda;               //!< Previous raw disk address
    word        blank;                	//!< always 0
    word        nbytes;                 //!< Number of bytes in page. First data page can be 0. Last data page can be < 512
    word        filepage;               //!< File relative page. 0 is leader page, 1 is first data page, and so on
    word        fid_file;               //!< 1 for all used files, ffff for free pages
    word        fid_dir;                //!< 8000 for a directory, 0 for regular, ffff for free
    word        fid_id;                 //!< file identifier, ffff for free
} afs_label_t;

typedef struct
{
    word        time[2];                //!< 32 bit time in seconds (based on what epoch?)
} afs_time_t;

typedef struct
{
    word        sn[2];                  //!< Serial number 32 bit (really?)
} afs_sn_t;

/**
 * @brief Structure of the file pointer
 */
typedef struct
{
    word        fid_dir;
    word        serialno;
    word        version;
    word        blank;
    word        leader_vda;
} afs_fp_t;

/**
 * @brief Structure of the file appendix
 */
typedef struct
{
    word        vda;                    //!< Virtual disk address
    word        filepage;               //!< file page (zero based)
    word        char_pos;               //!< Offset into the page (character position)
} afs_fa_t;

/**
 * @brief Structure of the leader page of each file
 * This is exactly 256 words (or 512 bytes) and defines
 * the structure of the first sector in each file.
 */
typedef struct
{
    afs_time_t  created;                //!< Time when created (ctime)
    afs_time_t  written;                //!< Time when last written to (mtime)
    afs_time_t  read;                   //!< Time when last read from (atime)
    char        filename[FNLEN];        //!< Pascal style string; all filenames have a trailing dot (.)
    word        leader_props[210];      //!< Property space (?)
    word        spare[10];              //!< Spare words for 256 word pages
    byte        proplength;             //!< 1 word
    byte        propbegin;              //!< offset into leader_props (?)
    byte        change_SN;              //!< 1 word (meaning is unknown)
    byte        consecutive;            //!< flag for consecutive images (?)
    afs_fp_t    dir_fp_hint;            //!< 5 words
    afs_fa_t    last_page_hint;         //!< 3 words
} afs_leader_t;

/**
 * @brief Structure of a directory entry (vector)
 *
 * The actual length of an entry is defined by the size of
 * typelength (1 word), fileptr (5 words) and the length
 * of the filename (filename[0] in big endian) in words,
 * i.e. words = ((filename[0] | 1) + 1) / 2;
 *
 * It seems that the criteria for the end of a directory
 * is that the filename length is 0 or greater than FNLEN.
 *
 * The typelength word is actually two bytes:
 * The type value is 4 for regular files, 0 for deleted entries.
 * The length value is most of the times somewhere close to the
 * filename length, but not the same.
 */
typedef struct
{
    byte        typelength[2];          //!< type and length
    afs_fp_t    fileptr;                //!< 5 words
    char        filename[FNLEN];        //!< not all used; filename[0] is the number of characters allocated
} afs_dv_t;

class afs_dv
{
public:
    afs_dv() : data() { memset(&data, 0, sizeof(data)); }
    afs_dv(const afs_dv_t& _dv) : data() { data = _dv; }
    afs_dv(const afs_dv& other) : data() { data = other.data; }
    afs_dv_t data;
};

/**
 * @brief Header for the DiskDescriptor file
 */
typedef struct
{
    word        nDisks;                 //!< How many disks in the file system
    word        nTracks;                //!< How big is each disk
    word        nHeads;                 //!< How many heads
    word        nSectors;               //!< How many sectors
    afs_sn_t    last_sn;                //!< Last SN used on disk
    word        blank;                  //!< Formerly bitTableChanged
    word        disk_bt_size;           //!< Number of valid words in the bit table
    word        def_versions_kept;      //!< 0 implies no multiple versions
    word        free_pages;             //!< Free pages left on the file system
    word        blank1[6];              //!< unused (0) space
} afs_kdh_t;

#endif // !defined(_AFS_TYPES_H_)
