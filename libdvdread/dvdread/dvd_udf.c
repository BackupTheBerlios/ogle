/**
 * This code is based on dvdudf by:
 *   Christian Wolff <scarabaeus@convergence.de>.
 *
 * Modifications by:
 *   Billy Biggs <vektor@dumbterm.net>.
 *
 * dvdudf: parse and read the UDF volume information of a DVD Video
 * Copyright (C) 1999 Christian Wolff for convergence integrated media
 * GmbH The author can be reached at scarabaeus@convergence.de, the
 * project's page is at http://linuxtv.org/dvd/
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  Or, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dvd_udf.h"


#ifndef u8
#define u8 unsigned char
#endif

#ifndef u16
#define u16 unsigned short
#endif

#ifndef u32
#define u32 unsigned long
#endif

#ifndef u64
#define u64 unsigned long long
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

struct Partition {
    int valid;
    char VolumeDesc[128];
    u16 Flags;
    u16 Number;
    char Contents[32];
    u32 AccessType;
    u32 Start;
    u32 Length;
} partition;

struct AD {
    u32 Location;
    u32 Length;
    u8 Flags;
    u16 Partition;
};

/* For direct data access, LSB first */
#define GETN1(p) ((u8)data[p])
#define GETN2(p) ((u16)data[p] | ((u16)data[(p) + 1] << 8))
#define GETN3(p) ((u32)data[p] | ((u32)data[(p) + 1] << 8) | ((u32)data[(p) + 2] << 16))
#define GETN4(p) ((u32)data[p] | ((u32)data[(p) + 1] << 8) | ((u32)data[(p) + 2] << 16) | ((u32)data[(p) + 3] << 24))
#define GETN(p, n, target) memcpy(target, &data[p], n)

static int Unicodedecode( u8 *data, int len, char *target ) 
{
    int p = 1, i = 0;

    if( ( data[ 0 ] == 8 ) || ( data[ 0 ] == 16 ) ) do {
        if( data[ 0 ] == 16 ) p++;  /* Ignore MSB of unicode16 */
        if( p < len ) {
            target[ i++ ] = data[ p++ ];
        }
    } while( p < len );

    target[ i ] = '\0';
    return 0;
}

static int UDFDescriptor( u8 *data, u16 *TagID ) 
{
    *TagID = GETN2(0);
    // TODO: check CRC 'n stuff
    return 0;
}

static int UDFExtentAD( u8 *data, u32 *Length, u32 *Location ) 
{
    *Length   = GETN4(0);
    *Location = GETN4(4);
    return 0;
}

static int UDFShortAD( u8 *data, struct AD *ad ) 
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(4);
    ad->Partition = partition.Number;  // use number of current partition
    return 0;
}

static int UDFLongAD( u8 *data, struct AD *ad )
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(4);
    ad->Partition = GETN2(8);
    //GETN(10, 6, Use);
    return 0;
}

static int UDFExtAD( u8 *data, struct AD *ad )
{
    ad->Length = GETN4(0);
    ad->Flags = ad->Length >> 30;
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GETN4(12);
    ad->Partition = GETN2(16);
    //GETN(10, 6, Use);
    return 0;
}

static int UDFICB( u8 *data, u8 *FileType, u16 *Flags )
{
    *FileType = GETN1(11);
    *Flags = GETN2(18);
    return 0;
}


static int UDFPartition( u8 *data, u16 *Flags, u16 *Number, char *Contents,
                         u32 *Start, u32 *Length )
{
    *Flags = GETN2(20);
    *Number = GETN2(22);
    GETN(24, 32, Contents);
    *Start = GETN4(188);
    *Length = GETN4(192);
    return 0;
}

/**
 * Reads the volume descriptor and checks the parameters.  Returns 0 on OK, 1
 * on error.
 */
static int UDFLogVolume( u8 *data, char *VolumeDescriptor )
{
    u32 lbsize, MT_L, N_PM;
    Unicodedecode(&data[84], 128, VolumeDescriptor);
    lbsize = GETN4(212);  // should be 2048
    MT_L = GETN4(264);    // should be 6
    N_PM = GETN4(268);    // should be 1
    if (lbsize != DVD_VIDEO_LB_LEN) return 1;
    return 0;
}

static int UDFFileEntry( u8 *data, u8 *FileType, struct AD *ad )
{
    u8 filetype;
    u16 flags;
    u32 L_EA, L_AD;
    int p;

    UDFICB( &data[ 16 ], &filetype, &flags );
    *FileType = filetype;
    L_EA = GETN4( 168 );
    L_AD = GETN4( 172 );
    p = 176 + L_EA;
    while( p < 176 + L_EA + L_AD ) {
        switch( flags & 0x0007 ) {
            case 0: UDFShortAD( &data[ p ], ad ); p += 8;  break;
            case 1: UDFLongAD( &data[ p ], ad );  p += 16; break;
            case 2: UDFExtAD( &data[ p ], ad );   p += 20; break;
            case 3:
                switch( L_AD ) {
                    case 8:  UDFShortAD( &data[ p ], ad ); break;
                    case 16: UDFLongAD( &data[ p ], ad );  break;
                    case 20: UDFExtAD( &data[ p ], ad );   break;
                }
                p += L_AD;
                break;
            default:
                p += L_AD; break;
        }
    }
    return 0;
}

static int UDFFileIdentifier( u8 *data, u8 *FileCharacteristics, char *FileName,
                              struct AD *FileICB )
{
    u8 L_FI;
    u16 L_IU;

    *FileCharacteristics = GETN1(18);
    L_FI = GETN1(19);
    UDFLongAD(&data[20], FileICB);
    L_IU = GETN2(36);
    if (L_FI) Unicodedecode(&data[38 + L_IU], L_FI, FileName);
    else FileName[0] = '\0';
    return 4 * ((38 + L_FI + L_IU + 3) / 4);
}

/**
 * Maps ICB to FileAD
 * ICB: Location of ICB of directory to scan
 * FileType: Type of the file
 * File: Location of file the ICB is pointing to
 * return 1 on success, 0 on error;
 */
static int UDFMapICB( int fd, struct AD ICB, u8 *FileType, struct AD *File ) 
{
    u8 LogBlock[DVD_VIDEO_LB_LEN];
    u32 lbnum;
    u16 TagID;

    lbnum = partition.Start + ICB.Location;
    do {
        if( UDFReadLB( fd, lbnum++, 1, LogBlock ) <= 0 ) {
            TagID = 0;
        } else {
            UDFDescriptor( LogBlock, &TagID );
        }

        if( TagID == 261 ) {
            UDFFileEntry( LogBlock, FileType, File );
            return 1;
        };
    } while( ( lbnum <= partition.Start + ICB.Location + ( ICB.Length - 1 )
             / DVD_VIDEO_LB_LEN ) && ( TagID != 261 ) );

    return 0;
}

/**
 * Dir: Location of directory to scan
 * FileName: Name of file to look for
 * FileICB: Location of ICB of the found file
 * return 1 on success, 0 on error;
 */
static int UDFScanDir( int fd, struct AD Dir, char *FileName,
                       struct AD *FileICB ) 
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    u8 directory[ 2 * DVD_VIDEO_LB_LEN ];
    u32 lbnum;
    u16 TagID;
    u8 filechar;
    int p;

    /* Scan dir for ICB of file */
    lbnum = partition.Start + Dir.Location;

    if( UDFReadLB( fd, lbnum, 2, directory ) <= 0 ) {
        return 0;
    }

    p = 0;
    while( p < Dir.Length ) {
        if( p > DVD_VIDEO_LB_LEN ) {
            ++lbnum;
            p -= DVD_VIDEO_LB_LEN;
            Dir.Length -= DVD_VIDEO_LB_LEN;
            if( UDFReadLB( fd, lbnum, 2, directory ) <= 0 ) {
                return 0;
            }
        }
        UDFDescriptor( &directory[ p ], &TagID );
        if( TagID == 257 ) {
            p += UDFFileIdentifier( &directory[ p ], &filechar,
                                    filename, FileICB );
            if( !strcasecmp( FileName, filename ) ) {
                return 1;
            }
        } else {
            return 0;
        }
    }

    return 0;
}

/**
 * Looks for partition on the disc.  Returns 1 if partition found, 0 on error.
 *   partnum: Number of the partition, starting at 0.
 *   part: structure to fill with the partition information
 */
static int UDFFindPartition( int fd, int partnum, struct Partition *part ) 
{
    u8 LogBlock[ DVD_VIDEO_LB_LEN ], Anchor[ DVD_VIDEO_LB_LEN ];
    u32 lbnum, MVDS_location, MVDS_length;
    u16 TagID;
    u32 lastsector;
    int i, terminate, volvalid;

    /* Find Anchor */
    lastsector = 0;
    lbnum = 256;   /* Try #1, prime anchor */
    terminate = 0;

    for(;;) {
        if( UDFReadLB( fd, lbnum, 1, Anchor ) > 0 ) {
            UDFDescriptor( Anchor, &TagID );
        } else {
            TagID = 0;
        }
        if (TagID != 2) {
            /* Not an anchor */
            if( terminate ) return 0; /* Final try failed */

            if( lastsector ) {

                /* We already found the last sector.  Try #3, alternative
                 * backup anchor.  If that fails, don't try again.
                 */
                lbnum = lastsector;
                terminate = 1;
            } else {
                /* TODO: Find last sector of the disc (this is optional). */
                if( lastsector ) {
                    /* Try #2, backup anchor */
                    lbnum = lastsector - 256;
                } else {
                    /* Unable to find last sector */
                    return 0;
                }
            }
        } else {
            /* It's an anchor! We can leave */
            break;
        }
    }
    /* Main volume descriptor */
    UDFExtentAD( &Anchor[ 16 ], &MVDS_length, &MVDS_location );
	
    part->valid = 0;
    volvalid = 0;
    part->VolumeDesc[ 0 ] = '\0';
    i = 1;
    do {
        /* Find Volume Descriptor */
        lbnum = MVDS_location;
        do {

            if( UDFReadLB( fd, lbnum++, 1, LogBlock ) <= 0 ) {
                TagID = 0;
            } else {
                UDFDescriptor( LogBlock, &TagID );
            }

            if( ( TagID == 5 ) && ( !part->valid ) ) {
                /* Partition Descriptor */
                UDFPartition( LogBlock, &part->Flags, &part->Number,
                              part->Contents, &part->Start, &part->Length );
                part->valid = ( partnum == part->Number );
            } else if( ( TagID == 6 ) && ( !volvalid ) ) {
                /* Logical Volume Descriptor */
                if( UDFLogVolume( LogBlock, part->VolumeDesc ) ) {  
                    /* TODO: sector size wrong! */
                } else {
                    volvalid = 1;
                }
            }

        } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
                 / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
                 && ( ( !part->valid ) || ( !volvalid ) ) );

        if( ( !part->valid) || ( !volvalid ) ) {
            /* Backup volume descriptor */
            UDFExtentAD( &Anchor[ 24 ], &MVDS_length, &MVDS_location );
        }
    } while( i-- && ( ( !part->valid ) || ( !volvalid ) ) );

    /* We only care for the partition, not the volume */
    return part->valid;
}

unsigned long int UDFFindFile( int fd, char *filename,
                               unsigned long int *filesize )
{
    u8 LogBlock[ DVD_VIDEO_LB_LEN ];
    u32 lbnum;
    u16 TagID;
    struct AD RootICB, File, ICB;
    char tokenline[ MAX_UDF_FILE_NAME_LEN ];
    char *token;
    u8 filetype;
    int Partition = 0;  /* Partition number to look for the file, 0 is the
                           standard location for DVD Video. */
	
    *filesize = 0;
    tokenline[0] = '\0';
    strcat( tokenline, filename );

    /* Find partition */
    if( !UDFFindPartition( fd, Partition, &partition ) ) return 0;

    /* Find root dir ICB */
    lbnum = partition.Start;
    do {
        if( UDFReadLB( fd, lbnum++, 1, LogBlock ) <= 0 ) {
            TagID = 0;
        } else {
            UDFDescriptor( LogBlock, &TagID );
        }

        /* File Set Descriptor */
        if( TagID == 256 ) {  // File Set Descriptor
            UDFLongAD( &LogBlock[ 400 ], &RootICB );
        }
    } while( ( lbnum < partition.Start + partition.Length )
             && ( TagID != 8 ) && ( TagID != 256 ) );

    /* Sanity checks. */
    if( TagID != 256 ) return 0;
    if( RootICB.Partition != Partition ) return 0;
	
    /* Find root dir */
    if( !UDFMapICB( fd, RootICB, &filetype, &File ) ) return 0;
    if( filetype != 4 ) return 0;  /* Root dir should be dir */

    /* Tokenize filepath */
    token = strtok(tokenline, "/");
    while( token != NULL ) {
        if( !UDFScanDir( fd, File, token, &ICB ) ) return 0;
        if( !UDFMapICB( fd, ICB, &filetype, &File ) ) return 0;
        token = strtok( NULL, "/" );
    }
    *filesize = File.Length;
    return partition.Start + File.Location;
}

int UDFReadLB( int fd, unsigned long int lb_number, unsigned int block_count,
               unsigned char *data )
{
    if( fd < 0 )
        return 0;

    lseek64( fd, (int64_t) lb_number * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET );

    return read( fd, data, block_count * DVD_VIDEO_LB_LEN );
}

