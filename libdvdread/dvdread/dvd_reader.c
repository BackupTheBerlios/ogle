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
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dlfcn.h>
#include <dirent.h>

#if defined(__sun)
#include <sys/mnttab.h>
#define MNT_FILE MNTTAB
#else
#include <mntent.h>
#define MNT_FILE MOUNTED
#endif

#include "dvd_udf.h"
#include "dvd_reader.h"

#ifndef CSS_TITLE_KEY_LEN
#define CSS_TITLE_KEY_LEN  5
#endif

void *css_handle = 0;
int (*CSSisEncrypted)( int );
int (*CSSAuthDisc)( int, char * );
int (*CSSAuthTitle)( int, char *, int );
int (*CSSDecryptTitleKey)( char *, char * );
void (*CSSDescramble)( unsigned char *, char * );

struct dvd_reader_s {
    /* Basic information. */
    int isEncrypted;
    int isImageFile;

    /* Information required for an image file. */
    int fd;
    char key_disc[ DVD_VIDEO_LB_LEN ];

    /* Information required for a directory path drive. */
    char *path_root;
    dvd_reader_t *auth_drive;
};

struct dvd_file_s {
    /* Basic information. */
    dvd_reader_t *dvd;
    int isEncrypted;
    char key_title[ CSS_TITLE_KEY_LEN ];

    /* Information required for an image file. */
    unsigned long int lb_start;
    off64_t seek_pos;

    /* Information required for a directory path drive. */
    int title_sizes[ 9 ];
    int title_fds[ 9 ];
};

static void setupCSS( void )
{
    if( !css_handle ) {
        css_handle = dlopen( "libcss.so", RTLD_NOW );
        if( !css_handle ) {
            fprintf( stderr, "libdvdread: Can't open libcss: %s.\n"
                     "Decryption of encrypted DVDs is unavailable.\n",
                     dlerror() );
        } else {
            CSSisEncrypted = (int (*)(int))
                                dlsym( css_handle, "CSSisEncrypted" );
            CSSAuthDisc = (int (*)(int, char*))
                                dlsym( css_handle, "CSSAuthDisc" );
            CSSAuthTitle = (int (*)(int, char*, int))
                                dlsym( css_handle, "CSSAuthTitle" );
            CSSDecryptTitleKey = (int (*)(char*, char*))
                                dlsym( css_handle, "CSSDecryptTitleKey" );
            CSSDescramble = (void (*)(unsigned char *, char*))
                                dlsym( css_handle, "CSSDescramble" );
        }
    }
}

/**
 * Open a DVD image or block device file.
 */
static dvd_reader_t *DVDOpenImageFile( const char *filename )
{
    dvd_reader_t *dvd;
    int fd;

    fd = open( filename, O_RDONLY );
    if( fd < 0 ) {
        fprintf( stderr, "libdvdread: Can't open %s for reading.\n",
                 filename );
        return 0;
    }

    dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
    if( !dvd ) return 0;
    dvd->isEncrypted = 0;
    dvd->isImageFile = 1;
    dvd->fd = fd;
    dvd->path_root = 0;
    dvd->auth_drive = 0;

    setupCSS();

    if( css_handle && CSSisEncrypted( fd ) ) {
        fprintf( stderr, "libdvdread: Encrypted DVD disc detected, "
                         "enabling CSS decryption.\n" );
        dvd->isEncrypted = 1;
        if( CSSAuthDisc( fd, dvd->key_disc ) < 0 ) {
            fprintf( stderr, "libdvdread: Can't authenticate disc, "
                             "disabling CSS decryption.\n" );
            dvd->isEncrypted = 0;
        }
    }

    return dvd;
}

static dvd_reader_t *DVDOpenPath( const char *path_root, dvd_reader_t *auth_drive )
{
    dvd_reader_t *dvd;

    dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
    if( !dvd ) return 0;
    dvd->isEncrypted = auth_drive ? 1 : 0;
    dvd->isImageFile = 0;
    dvd->fd = 0;
    dvd->path_root = strdup( path_root );
    dvd->auth_drive = auth_drive;

    setupCSS();

    if( auth_drive )
        fprintf( stderr, "libdvdread: CSS Authentication Enabled.\n" );

    return dvd;
}

dvd_reader_t *DVDOpen( const char *path )
{
    struct stat fileinfo;
    int ret;

    /* First check if this is a block device */
    ret = stat( path, &fileinfo );
    if( ret < 0 ) {
        /* If we can't stat the file, give up */
        fprintf( stderr, "libdvdread: Can't open %s.\n", path );
        return 0;
    }

    if( S_ISBLK( fileinfo.st_mode ) || S_ISREG( fileinfo.st_mode ) ) {

        /**
         * Block devices and regular files are assumed to be DVD-Video images.
         */
        return DVDOpenImageFile( path );

    } else if( S_ISDIR( fileinfo.st_mode ) ) {
        dvd_reader_t *auth_drive = 0;
        char *path_copy = strdup( path );
        FILE *mntfile;

        /**
         * If we're being asked to open a directory, check if that directory is
         * maybe the mountpoint for a DVD-ROM which we can use to negotiate CSS
         * authentication.
         */

        /* XXX: We should scream real loud here. */
        if( !path_copy ) return 0;

        if( strlen( path_copy ) > 1 ) {
            if( path[ strlen( path_copy ) - 1 ] == '/' ) path_copy[ strlen( path_copy ) - 1 ] = '\0';
        }

        if( strlen( path_copy ) > 9 ) {
            if( !strcasecmp( &(path_copy[ strlen( path_copy ) - 9 ]), "/video_ts" ) ) {
                path_copy[ strlen( path_copy ) - 9 ] = '\0';
            }
        }

        mntfile = fopen( MNT_FILE, "r" );
        if( mntfile ) {
#if defined(__sun)
            struct mnttab mp;
            int res;
   
            while( ( res = getmntent( mntfile, &mp ) ) != -1 ) {
                if( res == 0 && !strcmp( mp.mnt_mountp, path_copy ) ) {
                    fprintf( stderr, "libdvdread: Attempting to use block "
                             "device %s for CSS authentication.\n",
                             mp.mnt_special );
                    auth_drive = DVDOpenImageFile( mp.mnt_special );
                    break;
                }
            }
#else
            struct mntent *me;
 
            while( ( me = getmntent( mntfile ) ) ) {
                if( !strcmp( me->mnt_dir, path_copy ) ) {
                    fprintf( stderr, "libdvdread: Attempting to use block "
                             "device %s for CSS authentication.\n",
                             me->mnt_fsname );
                    auth_drive = DVDOpenImageFile( me->mnt_fsname );
                    break;
                }
            }
#endif
            fclose( mntfile );
        }

        free( path_copy );

        /**
         * We now have enough information to open the path.
         */
        return DVDOpenPath( path, auth_drive );
    }

    /* If it's none of the above, screw it. */
    fprintf( stderr, "libdvdread: Can't open path %s.\n", path );
    return 0;
}

void DVDClose( dvd_reader_t *dvd )
{
    if( dvd->fd ) close( dvd->fd );
    if( dvd->path_root ) free( dvd->path_root );
    if( dvd->auth_drive ) DVDClose( dvd->auth_drive );
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

    start = UDFFindFile( dvd->fd, filename, &len );
    if( !start ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = start;
    dvd_file->isEncrypted = 0;
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

    /* Get the full path of the file */
    if( !findDVDFile( dvd, filename, full_path ) ) return 0;

    fd = open( full_path, O_RDONLY );
    if( fd < 0 ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = 0;
    dvd_file->isEncrypted = 0;

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

static dvd_file_t *DVDOpenVOBUDF( dvd_reader_t *dvd, unsigned int title, int menu )
{
    char filename[ MAX_UDF_FILE_NAME_LEN ];
    unsigned long int start, len;
    dvd_file_t *dvd_file;

    if( title == 0 ) {
        sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
    } else {
        sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
    }
    start = UDFFindFile( dvd->fd, filename, &len );
    if( start == 0 ) return 0;

    dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
    if( !dvd_file ) return 0;
    dvd_file->dvd = dvd;
    dvd_file->lb_start = start;
    dvd_file->seek_pos = 0;

    if( css_handle && dvd->isEncrypted ) {
        dvd_file->isEncrypted = 1;

        if( CSSAuthTitle( dvd->fd, dvd_file->key_title,
                          dvd_file->lb_start ) >= 0 ) {
            if( CSSDecryptTitleKey( dvd_file->key_title,
                                    dvd->key_disc ) >= 0 ) {
            } else {
                fprintf( stderr, "libdvdread: Couldn't get title key "
                                 "for title %d%s.\n", title,
                                 menu ? " [Menu]" : "" );
                dvd_file->isEncrypted = 0;
            }
        } else {
            fprintf( stderr, "libdvdread: Couldn't authenticate title %d.\n",
                     title );
            dvd_file->isEncrypted = 0;
        }
    }

    return dvd_file;
}

static dvd_file_t *DVDOpenVOBPath( dvd_reader_t *dvd, unsigned int title, int menu )
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
    dvd_file->isEncrypted = dvd->auth_drive ? 1 : 0;
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

    if( dvd_file->isEncrypted ) {
        dvd_file_t *title_file;

        /**
         * What we do now is use the auth drive to open the same file, grab the
         * key, and then close the file.
         */

        title_file = DVDOpenFile( dvd->auth_drive, title,
                                  menu ? DVD_READ_MENU_VOBS : DVD_READ_TITLE_VOBS );
        if( title_file ) {
            memcpy( dvd_file->key_title, title_file->key_title, CSS_TITLE_KEY_LEN );
            DVDCloseFile( title_file );
        } else {
            fprintf( stderr, "libdvdread: Can't get CSS key for title %d%s.\n",
                     title, menu ? " [Menu]" : "" );
            dvd_file->isEncrypted = 0;
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

static int DVDReadBlocksUDF( dvd_file_t *dvd_file, size_t offset,
                             size_t block_count, unsigned char *data )
{
    int len, i;

    len = UDFReadLB( dvd_file->dvd->fd, dvd_file->lb_start + offset,
                     block_count, data );

    if( len != block_count * DVD_VIDEO_LB_LEN ) {
        return len;
    }

    if( css_handle && dvd_file->isEncrypted ) {
        for( i = 0; i < block_count; ++i ) {
            CSSDescramble( data + ( DVD_VIDEO_LB_LEN * i ),
                           dvd_file->key_title );
        }
    }

    return len;
}

static int DVDReadBlocksPath( dvd_file_t *dvd_file, size_t offset,
                              size_t block_count, unsigned char *data )
{
    int i, ret;

    ret = 0;
    for( i = 0; i < 9; ++i ) {
        if( !dvd_file->title_sizes[ i ] ) return 0;

        if( offset < dvd_file->title_sizes[ i ] ) {
            if( ( offset + block_count ) <= dvd_file->title_sizes[ i ] ) {
                lseek64( dvd_file->title_fds[ i ], offset
                         * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET );
                ret = read( dvd_file->title_fds[ i ], data,
                            block_count * DVD_VIDEO_LB_LEN );
                break;
            } else {

                /* Read part 1 */
                lseek64( dvd_file->title_fds[ i ], offset
                         * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET );
                ret = read( dvd_file->title_fds[ i ], data,
                            dvd_file->title_sizes[ i ] - offset );

                /* Read part 2 */
                lseek64( dvd_file->title_fds[ i + 1 ], 0, SEEK_SET );
                ret += read( dvd_file->title_fds[ i + 1 ],
                             &(data[ ( dvd_file->title_sizes[ i ] - offset )
                             * DVD_VIDEO_LB_LEN ]),
                             block_count - ( dvd_file->title_sizes[ i ] 
                                         - offset ) );
                break;
            }
        } else {
            offset -= dvd_file->title_sizes[ i ];
        }
    }

    if( ret && css_handle && dvd_file->isEncrypted ) {
        for( i = 0; i < block_count; ++i ) {
            CSSDescramble( data + ( DVD_VIDEO_LB_LEN * i ),
                           dvd_file->key_title );
        }
    }

    return ret;
}

int DVDReadBlocks( dvd_file_t *dvd_file, size_t offset, size_t block_count,
                   unsigned char *data )
{
    if( dvd_file->dvd->isImageFile ) {
        return DVDReadBlocksUDF( dvd_file, offset, block_count, data );
    } else {
        return DVDReadBlocksPath( dvd_file, offset, block_count, data );
    }
}

off64_t DVDFileSeek( dvd_file_t *dvd_file, off64_t offset )
{
    if( dvd_file->dvd->isImageFile ) {
        dvd_file->seek_pos = offset;
        return offset;
    } else {
        return lseek64( dvd_file->title_fds[ 0 ], offset, SEEK_SET );
    }
}

static int DVDReadBytesUDF( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
    unsigned char *secbuf;
    int numsec, len, i;
    int seek_sector, seek_byte;

    seek_sector = ( (int) ( dvd_file->seek_pos
                            / (int64_t) DVD_VIDEO_LB_LEN ) );
    seek_byte   = ( (int) ( dvd_file->seek_pos
                            % (int64_t) DVD_VIDEO_LB_LEN ) );

    numsec = ( ( seek_byte + byte_size ) / DVD_VIDEO_LB_LEN ) + 1;
    secbuf = (unsigned char *) malloc( numsec * DVD_VIDEO_LB_LEN );
    if( !secbuf ) {
        fprintf( stderr, "libdvdread: Can't allocate memory for file read!\n" );
        return 0;
    }

    len = UDFReadLB( dvd_file->dvd->fd, dvd_file->lb_start + seek_sector,
                     numsec, secbuf );
    if( len != numsec * DVD_VIDEO_LB_LEN ) {
        free( secbuf );
        return 0;
    }

    dvd_file->seek_pos += byte_size;

    if( css_handle && dvd_file->isEncrypted ) {
        for( i = 0; i < numsec; ++i ) {
            CSSDescramble( secbuf + ( DVD_VIDEO_LB_LEN * i ), dvd_file->key_title );
        }
    }

    memcpy( data, &(secbuf[ seek_byte ]), byte_size );
    free( secbuf );

    return byte_size;
}

static int DVDReadBytesPath( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
    return read( dvd_file->title_fds[ 0 ], data, byte_size );
}

int DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
    if( dvd_file->dvd->isImageFile ) {
        return DVDReadBytesUDF( dvd_file, data, byte_size );
    } else {
        return DVDReadBytesPath( dvd_file, data, byte_size );
    }
}

