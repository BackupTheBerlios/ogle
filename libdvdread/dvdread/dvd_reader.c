/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dlfcn.h>
#include <dirent.h>

#if defined(__sun)
#include <sys/mnttab.h>
#define MNT_FILE MNTTAB
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#undef MNT_FILE
#include <fstab.h>
#elif defined(__linux__)
#include <mntent.h>
#define MNT_FILE MOUNTED
#else /* Unknown system */
#define MNT_FILE "/dev/zero"
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
typedef off_t off64_t;
#define lseek64 lseek
#endif

#include "dvdcss.h"
#include "dvd_udf.h"
#include "dvd_reader.h"

/**
 * Handle to the loaded dvdcss library.
 */
void *dvdcss_library = 0;

/**
 * libdvdcss functions.
 */
static dvdcss_handle (*dvdcss_open)  ( char *psz_target,
                                       int i_flags );
static int           (*dvdcss_close) ( dvdcss_handle );
static int           (*dvdcss_title) ( dvdcss_handle,
                                       int i_block );
static int           (*dvdcss_seek)  ( dvdcss_handle,
                                       int i_blocks );
static int           (*dvdcss_read)  ( dvdcss_handle,
                                       void *p_buffer,
                                       int i_blocks,
                                       int i_title );
static char *        (*dvdcss_error) ( dvdcss_handle );

struct dvd_reader_s {
    /* Basic information. */
    int isImageFile;

    /* Information required for an image file. */
    dvdcss_handle dev;
    int fd;

    /* Information required for a directory path drive. */
    char *path_root;
};

struct dvd_file_s {
    /* Basic information. */
    dvd_reader_t *dvd;

    /* Information required for an image file. */
    unsigned long int lb_start;
    uint32_t seek_pos;

    /* Information required for a directory path drive. */
    size_t title_sizes[ 9 ];
    int title_fds[ 9 ];
};

static void setupCSS( void )
{
    if( !dvdcss_library ) {
	dvdcss_library = dlopen( "libdvdcss.so", RTLD_LAZY );

	if( !dvdcss_library ) {
            fprintf( stderr, "libdvdread: Can't open libdvdcss: %s.\n",
                     dlerror() );
        } else {
#if defined(__OpenBSD__)
#define U_S "_"
#else
#define U_S
#endif
            dvdcss_open = (dvdcss_handle (*)(char*, int))
                                dlsym( dvdcss_library, U_S "dvdcss_open" );
            dvdcss_close = (int (*)(dvdcss_handle))
                                dlsym( dvdcss_library, U_S "dvdcss_close" );
            dvdcss_title = (int (*)(dvdcss_handle, int))
                                dlsym( dvdcss_library, U_S "dvdcss_title" );
            dvdcss_seek = (int (*)(dvdcss_handle, int))
                                dlsym( dvdcss_library, U_S "dvdcss_seek" );
            dvdcss_read = (int (*)(dvdcss_handle, void*, int, int))
                                dlsym( dvdcss_library, U_S "dvdcss_read" );
            dvdcss_error = (char* (*)(dvdcss_handle))
                                dlsym( dvdcss_library, U_S "dvdcss_error" );

            if( dlsym( dvdcss_library, U_S "dvdcss_crack" ) ) {
                fprintf( stderr, "libdvdread: Old (pre-0.0.2) version of "
                                 "libdvdcss found.\n"
                                 "libdvdread: You should get the "
                                 "latest version from "
                                 "http://www.videolan.org/\n" );
                dlclose( dvdcss_library );
                dvdcss_library = 0;
            } else if( !dvdcss_open  || !dvdcss_close || !dvdcss_seek ||
                       !dvdcss_title || !dvdcss_read  || !dvdcss_error ) {

                fprintf( stderr, "libdvdread: Unknown incompatible version "
                                 "of libdvdcss found.\n"
                                 "libdvdread: Try to find a "
                                 "newer version of libdvdread?\n" );
                dlclose( dvdcss_library );
                dvdcss_library = 0;
            }
        }
    }

    if( !dvdcss_library ) {
        fprintf( stderr, "libdvdread: Encrypted DVD support unavailable.\n" );
    }
}

/**
 * Open a DVD image or block device file.
 */
static dvd_reader_t *DVDOpenImageFile( const char *filename )
{
    dvd_reader_t *dvd;
    dvdcss_handle dev = 0;
    int fd = 0;

    setupCSS();

    if( dvdcss_library ) {
        dev = dvdcss_open( (char *) filename, DVDCSS_INIT_DEBUG );
        if( !dev ) {
            fprintf( stderr, "libdvdread: Can't open %s for reading.\n",
                     filename );
            return 0;
        }
    } else {
        fd = open( filename, O_RDONLY );
        if( fd < 0 ) {
            fprintf( stderr, "libdvdread: Can't open %s for reading.\n",
                     filename );
            return 0;
        }
    }

    dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
    if( !dvd ) return 0;
    dvd->isImageFile = 1;
    dvd->dev = dev;
    dvd->fd = fd;
    dvd->path_root = 0;

    return dvd;
}

static dvd_reader_t *DVDOpenPath( const char *path_root )
{
    dvd_reader_t *dvd;

    dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
    if( !dvd ) return 0;
    dvd->isImageFile = 0;
    dvd->dev = 0;
    dvd->path_root = strdup( path_root );

    return dvd;
}

dvd_reader_t *DVDOpen( const char *path )
{
    struct stat fileinfo;
    int ret;

    if( !path ) return 0;
    
    ret = stat( path, &fileinfo );
    if( ret < 0 ) {
        /* If we can't stat the file, give up */
        fprintf( stderr, "libdvdread: Can't open %s.\n", path );
        return 0;
    }

    /* First check if this is a block/char device or a file*/
    if( S_ISBLK( fileinfo.st_mode ) || 
	S_ISCHR( fileinfo.st_mode ) || 
	S_ISREG( fileinfo.st_mode ) ) {

        /**
         * Block devices and regular files are assumed to be DVD-Video images.
         */
        return DVDOpenImageFile( path );

    } else if( S_ISDIR( fileinfo.st_mode ) ) {
        dvd_reader_t *auth_drive = 0;
        char *path_copy = strdup( path );
#if defined(__FreeBSD__) || defined(__OpenBSD__)
        struct fstab* fe;
#else
        FILE *mntfile;
#endif
	
        /**
         * If we're being asked to open a directory, check if that directory
	 * is the mountpoint for a DVD-ROM which we can use instead.
         */

        /* XXX: We should scream real loud here. */
        if( !path_copy ) return 0;

        if( strlen( path_copy ) > 1 ) {
	    if( path[ strlen( path_copy ) - 1 ] == '/' ) 
	        path_copy[ strlen( path_copy ) - 1 ] = '\0';
        }

        if( strlen( path_copy ) > 9 ) {
	    if( !strcasecmp( &(path_copy[ strlen( path_copy ) - 9 ]), 
			     "/video_ts" ) ) {
                path_copy[ strlen( path_copy ) - 9 ] = '\0';
            }
        }

#if defined(__FreeBSD__) || defined(__OpenBSD__)
        if( ( fe = getfsfile( path_copy ) ) ) {
	    fprintf( stderr,
		     "libdvdread: Attempting to use block "
		     "device %s on %s for CSS authentication.\n",
		     fe->fs_spec,
		     fe->fs_file );
	    auth_drive = DVDOpenImageFile( fe->fs_spec );
        }
#else
        mntfile = fopen( MNT_FILE, "r" );
        if( mntfile ) {
#if defined(__sun)
            struct mnttab mp;
            int res;
   
            while( ( res = getmntent( mntfile, &mp ) ) != -1 ) {
                if( res == 0 && !strcmp( mp.mnt_mountp, path_copy ) ) {
                    fprintf( stderr, "libdvdread: Attempting to use block "
                             "device %s on %s for CSS authentication.\n",
                             mp.mnt_special,
			     mp.mnt_mountp );
                    auth_drive = DVDOpenImageFile( mp.mnt_special );
                    break;
                }
            }
#elif defined(__linux__)
            struct mntent *me;
 
            while( ( me = getmntent( mntfile ) ) ) {
                if( !strcmp( me->mnt_dir, path_copy ) ) {
                    fprintf( stderr, "libdvdread: Attempting to use block "
                             "device %s on %s for CSS authentication.\n",
                             me->mnt_fsname,
			     me->mnt_dir );
                    auth_drive = DVDOpenImageFile( me->mnt_fsname );
                    break;
                }
            }
#endif
            fclose( mntfile );
	}
#endif
	if( !auth_drive ) {
            fprintf( stderr, "libdvdread: Block device unacessable, "
		     "CSS authentication not available.\n" );
	}

        free( path_copy );

        /**
         * If we've opened a drive, just use that.
         */
        if( auth_drive ) return auth_drive;

        /**
         * Otherwise, we now open the directory tree.
         */
        return DVDOpenPath( path );
    }

    /* If it's none of the above, screw it. */
    fprintf( stderr, "libdvdread: Can't open path %s.\n", path );
    return 0;
}

void DVDClose( dvd_reader_t *dvd )
{
    if( dvd->dev ) dvdcss_close( dvd->dev );
    if( dvd->fd ) close( dvd->fd );
    if( dvd->path_root ) free( dvd->path_root );
    free( dvd );
    dvd = 0;
}

/**
 * Open an unencrypted file on a DVD image file.
 */
static dvd_file_t *DVDOpenFileUDF( dvd_reader_t *dvd, char *filename )
{
    unsigned long int start, len;
    dvd_file_t *dvd_file;

    start = UDFFindFile( dvd, filename, &len );
    if( !start ) {
        fprintf( stderr, "libdvdread: Can't find file %s via UDF.\n",
                 filename );
        return 0;
    }

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = start;
    dvd_file->seek_pos = 0;

    return dvd_file;
}

/**
 * Searches for <file> in directory <path>, ignoring case.
 * Returns 0 and full filename in <filename>.
 *     or -1 on file not found.
 *     or -2 on path not found.
 */
static int findDirFile( const char *path, const char *file, char *filename ) 
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir( path );
    if( !dir ) return -2;

    while( ( ent = readdir( dir ) ) != NULL ) {
        if( !strcasecmp( ent->d_name, file ) ) {
            sprintf( filename, "%s%s%s", path,
                     ( ( path[ strlen( path ) - 1 ] == '/' ) ? "" : "/" ),
                     ent->d_name );
            return 0;
        }
    }

    return -1;
}

static int findDVDFile( dvd_reader_t *dvd, const char *file, char *filename )
{
    char video_path[ PATH_MAX + 1 ];
    const char *nodirfile;
    int ret;

    /* Strip off the directory for our search */
    if( !strncasecmp( "/VIDEO_TS/", file, 10 ) ) {
        nodirfile = &(file[ 10 ]);
    } else {
        nodirfile = file;
    }

    ret = findDirFile( dvd->path_root, nodirfile, filename );
    if( ret < 0 ) {
        /* Try also with adding the path, just in case. */
        sprintf( video_path, "%s/VIDEO_TS/", dvd->path_root );
        ret = findDirFile( video_path, nodirfile, filename );
        if( ret < 0 ) {
            /* Try with the path, but in lower case. */
            sprintf( video_path, "%s/video_ts/", dvd->path_root );
            ret = findDirFile( video_path, nodirfile, filename );
            if( ret < 0 ) {
                return 0;
            }
        }
    }

    return 1;
}

/**
 * Open an unencrypted file from a DVD directory tree.
 */
static dvd_file_t *DVDOpenFilePath( dvd_reader_t *dvd, char *filename )
{
    char full_path[ PATH_MAX + 1 ];
    dvd_file_t *dvd_file;
    struct stat fileinfo;
    int fd;

    /* Get the full path of the file. */
    if( !findDVDFile( dvd, filename, full_path ) ) return 0;

    fd = open( full_path, O_RDONLY );
    if( fd < 0 ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = 0;

    if( stat( full_path, &fileinfo ) < 0 ) {
        fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
        free( dvd_file );
        return 0;
    }
    dvd_file->title_sizes[ 0 ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
    dvd_file->title_sizes[ 1 ] = 0;
    dvd_file->title_fds[ 0 ] = fd;

    return dvd_file;
}

static dvd_file_t *DVDOpenVOBUDF( dvd_reader_t *dvd, 
				  int title, int menu )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    long int start;
    unsigned long int len;
    dvd_file_t *dvd_file;

    if( title == 0 ) {
        sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
    } else {
        sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
    }
    start = UDFFindFile( dvd, filename, &len );
    if( start == 0 ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = start;
    dvd_file->seek_pos = 0;

    /* Perform CSS key cracking for this title. */
    if( dvdcss_library ) {
        if( dvdcss_title( dvd_file->dvd->dev, start ) < 0 ) {
            fprintf( stderr, "libdvdread: Error cracking CSS key for %s\n",
                     filename );
        }
    }

    return dvd_file;
}

static dvd_file_t *DVDOpenVOBPath( dvd_reader_t *dvd, 
				   int title, int menu )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    char full_path[ PATH_MAX + 1 ];
    struct stat fileinfo;
    dvd_file_t *dvd_file;
    int i;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = 0;
    dvd_file->seek_pos = 0;

    if( menu ) {
        int fd;

        dvd_file->title_sizes[ 0 ] = 0;
        dvd_file->title_fds[ 0 ] = 0;

        if( title == 0 ) {
            sprintf( filename, "VIDEO_TS.VOB" );
        } else {
            sprintf( filename, "VTS_%02i_0.VOB", title );
        }
        if( !findDVDFile( dvd, filename, full_path ) ) {
            free( dvd_file );
            return 0;
        }

        fd = open( full_path, O_RDONLY );
        if( fd < 0 ) {
            free( dvd_file );
            return 0;
        }

        if( stat( full_path, &fileinfo ) < 0 ) {
            fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
            free( dvd_file );
            return 0;
        }
        dvd_file->title_sizes[ 0 ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
        dvd_file->title_sizes[ 1 ] = 0;
        dvd_file->title_fds[ 0 ] = fd;

    } else {
        for( i = 0; i < 9; ++i ) {
            dvd_file->title_sizes[ i ] = 0;
            dvd_file->title_fds[ i ] = 0;

            sprintf( filename, "VTS_%02i_%i.VOB", title, i + 1 );
            if( !findDVDFile( dvd, filename, full_path ) ) {
                break;
            }

            if( stat( full_path, &fileinfo ) < 0 ) {
                fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
                break;
            }

            dvd_file->title_sizes[ i ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
            dvd_file->title_fds[ i ] = open( full_path, O_RDONLY );
        }
        if( !(dvd_file->title_sizes[ 0 ]) ) {
            free( dvd_file );
            return 0;
        }
    }

    return dvd_file;
}

dvd_file_t *DVDOpenFile( dvd_reader_t *dvd, int titlenum, 
			 dvd_read_domain_t domain )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];

    switch( domain ) {
    case DVD_READ_INFO_FILE:
        if( titlenum == 0 ) {
            sprintf( filename, "/VIDEO_TS/VIDEO_TS.IFO" );
        } else {
            sprintf( filename, "/VIDEO_TS/VTS_%02i_0.IFO", titlenum );
        }
        break;
    case DVD_READ_INFO_BACKUP_FILE:
        if( titlenum == 0 ) {
            sprintf( filename, "/VIDEO_TS/VIDEO_TS.BUP" );
        } else {
            sprintf( filename, "/VIDEO_TS/VTS_%02i_0.BUP", titlenum );
        }
        break;
    case DVD_READ_MENU_VOBS:
        if( dvd->isImageFile ) {
            return DVDOpenVOBUDF( dvd, titlenum, 1 );
        } else {
            return DVDOpenVOBPath( dvd, titlenum, 1 );
        }
        break;
    case DVD_READ_TITLE_VOBS:
        if( titlenum == 0 ) return 0;
        if( dvd->isImageFile ) {
            return DVDOpenVOBUDF( dvd, titlenum, 0 );
        } else {
            return DVDOpenVOBPath( dvd, titlenum, 0 );
        }
        break;
    default:
        fprintf( stderr, "libdvdread: Invalid domain for file open.\n" );
        return 0;
    }
    
    if( dvd->isImageFile ) {
        return DVDOpenFileUDF( dvd, filename );
    } else {
        return DVDOpenFilePath( dvd, filename );
    }
}

void DVDCloseFile( dvd_file_t *dvd_file )
{
    int i;

    if( !dvd_file->dvd->isImageFile ) {
        for( i = 0; i < 9; ++i ) {
            if( !dvd_file->title_fds[ i ] ) break;
            close( dvd_file->title_fds[ i ] );
        }
    }

    free( dvd_file );
    dvd_file = 0;
}

int64_t DVDReadLBUDF( dvd_reader_t *device, uint32_t lb_number,
		      size_t block_count, unsigned char *data, 
		      int encrypted )
{
    if( dvdcss_library ) {
        int ret;

        if( !device->dev ) {
            fprintf( stderr, "libdvdread: Fatal error in block read.\n" );
            return 0;
        }

        ret = dvdcss_seek( device->dev, (int) lb_number );
        if( ret != (int) lb_number ) {
	    fprintf( stderr, "libdvdread: Can't seek to block %u\n", 
		     lb_number );
            return 0;
        }

        return ( dvdcss_read( device->dev, (char *) data, (int) block_count, 
			      encrypted ) * (uint64_t)DVD_VIDEO_LB_LEN );
    } else {
        off64_t off;

        if( !device->fd ) {
            fprintf( stderr, "libdvdread: Fatal error in block read.\n" );
            return 0;
        }

        off = lseek64( device->fd, lb_number * (int64_t) DVD_VIDEO_LB_LEN, 
		       SEEK_SET );
        if( off != ( lb_number * (int64_t) DVD_VIDEO_LB_LEN ) ) {
	    fprintf( stderr, "libdvdread: Can't seek to block %u\n", 
		     lb_number );
            return 0;
        }
        return read( device->fd, data, block_count * DVD_VIDEO_LB_LEN );
    }
}

static int64_t DVDReadBlocksUDF( dvd_file_t *dvd_file, uint32_t offset,
				 size_t block_count, unsigned char *data )
{
    return DVDReadLBUDF( dvd_file->dvd, dvd_file->lb_start + offset,
                         block_count, data, 1 );
}

static int64_t DVDReadBlocksPath( dvd_file_t *dvd_file, uint32_t offset,
				  size_t block_count, unsigned char *data )
{
    int i;
    ssize_t ret, ret2;
    off64_t off;

    ret = 0;
    ret2 = 0;
    for( i = 0; i < 9; ++i ) {
        if( !dvd_file->title_sizes[ i ] ) return 0;

        if( offset < dvd_file->title_sizes[ i ] ) {
            if( ( offset + block_count ) <= dvd_file->title_sizes[ i ] ) {
	        off = lseek64( dvd_file->title_fds[ i ], 
			       offset * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET );
		if( off != ( offset * (int64_t) DVD_VIDEO_LB_LEN ) ) {
		    fprintf( stderr, "libdvdread: Can't seek to block %u\n", 
			     offset );
		    return 0;
		}
                ret = read( dvd_file->title_fds[ i ], data,
                            block_count * DVD_VIDEO_LB_LEN );
                break;
            } else {
		size_t part1_size 
		  = ( dvd_file->title_sizes[ i ] - offset ) * DVD_VIDEO_LB_LEN;
		/* FIXME: Really needs to be a while loop.
		   (This is only true if you try and read >1GB at a time) */
		
                /* Read part 1 */
                off = lseek64( dvd_file->title_fds[ i ], 
			       offset * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET );
		if( off != ( offset * (int64_t) DVD_VIDEO_LB_LEN ) ) {
		    fprintf( stderr, "libdvdread: Can't seek to block %u\n", 
			     offset );
		    return 0;
		}
                ret = read( dvd_file->title_fds[ i ], data, part1_size );
		if( ret < 0 ) return ret;
		/* FIXME: This is wrong if i is the last file in the set. 
		          also error from this read will not show in ret. */
		
                /* Read part 2 */
                lseek64( dvd_file->title_fds[ i + 1 ], (off64_t)0, SEEK_SET );
                ret2 = read( dvd_file->title_fds[ i + 1 ], data + part1_size,
                             block_count * DVD_VIDEO_LB_LEN - part1_size );
                if( ret2 < 0 ) return ret2;
		break;
            }
        } else {
            offset -= dvd_file->title_sizes[ i ];
        }
    }

    return ( (int64_t) ret + (int64_t) ret2 );
}

/* These are broken for some cases reading more than 2Gb at a time. */
ssize_t DVDReadBlocks( dvd_file_t *dvd_file, uint32_t offset, 
		       size_t block_count, unsigned char *data )
{
    int64_t ret;
  
    if( dvd_file->dvd->isImageFile ) {
        ret = DVDReadBlocksUDF( dvd_file, offset, block_count, data );
    } else {
        ret = DVDReadBlocksPath( dvd_file, offset, block_count, data );
    }
    if( ret < 0 ) {
        return (ssize_t) ret;
    }
    return (ssize_t) (ret / DVD_VIDEO_LB_LEN );
}

int32_t DVDFileSeek( dvd_file_t *dvd_file, int32_t offset )
{
    if( dvd_file->dvd->isImageFile ) {
        dvd_file->seek_pos = (uint32_t) offset;
        return offset;
    } else {
        return (int32_t) ( lseek( dvd_file->title_fds[ 0 ], 
				  (off_t) offset, SEEK_SET ) );
    }
}

static ssize_t DVDReadBytesUDF( dvd_file_t *dvd_file, void *data, 
				size_t byte_size )
{
    unsigned char *secbuf;
    unsigned int numsec, seek_sector, seek_byte;
    int64_t len;
    
    seek_sector = dvd_file->seek_pos / DVD_VIDEO_LB_LEN;
    seek_byte   = dvd_file->seek_pos % DVD_VIDEO_LB_LEN;

    numsec = ( ( seek_byte + byte_size ) / DVD_VIDEO_LB_LEN ) + 1;
    secbuf = (unsigned char *) malloc( numsec * DVD_VIDEO_LB_LEN );
    if( !secbuf ) {
	fprintf( stderr, "libdvdread: Can't allocate memory " 
		 "for file read!\n" );
        return 0;
    }

    len = DVDReadLBUDF( dvd_file->dvd, dvd_file->lb_start + seek_sector,
                        numsec, secbuf, 0 );
    if( len != numsec * (int64_t) DVD_VIDEO_LB_LEN ) {
        free( secbuf );
        return 0;
    }

    dvd_file->seek_pos += byte_size;

    memcpy( data, &(secbuf[ seek_byte ]), byte_size );
    free( secbuf );

    return byte_size;
}

static ssize_t DVDReadBytesPath( dvd_file_t *dvd_file, void *data, 
				 size_t byte_size )
{
    return read( dvd_file->title_fds[ 0 ], data, byte_size );
}

ssize_t DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
    if( dvd_file->dvd->isImageFile ) {
        return DVDReadBytesUDF( dvd_file, data, byte_size );
    } else {
        return DVDReadBytesPath( dvd_file, data, byte_size );
    }
}

