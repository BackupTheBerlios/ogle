/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>,
 *                    H�kan Hjort <d95hjort@dtek.chalmers.se>
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

#ifndef DVD_READER_H_INCLUDED
#define DVD_READER_H_INCLUDED

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dvd_reader_s dvd_reader_t;
typedef struct dvd_file_s dvd_file_t;

/**
 * Opens a block device of a DVD-ROM file, or an image file, or a directory
 * name for a mounted DVD or HD copy of a DVD.  Returns 0 if we can't get any
 * of those methods to work.
 *
 * If the given file is a block device, or is the mountpoint for a block
 * device, then that device is used for CSS authentication using libcss.  If no
 * device is available, then no CSS authentication is performed, and we hope
 * that the image is decrypted.
 *
 * If the path given is a directory, then the files in that directory may be in
 * any one of these formats:
 *
 *   path/VIDEO_TS/VTS_01_1.VOB
 *   path/video_ts/vts_01_1.vob
 *   path/VTS_01_1.VOB
 *   path/vts_01_1.vob
 */
dvd_reader_t *DVDOpen( const char *path );

/**
 * Closes and cleans up the DVD reader object.  You must close all open files
 * before calling this function.
 */
void DVDClose( dvd_reader_t *dvd );

/**
 * INFO_FILE       : VIDEO_TS.IFO     (manager)
 *                   VTS_XX_0.IFO     (title)
 *
 * INFO_BACKUP_FILE: VIDEO_TS.BUP     (manager)
 *                   VTS_XX_0.BUP     (title)
 *
 * MENU_VOBS       : VIDEO_TS.VOB     (manager)
 *                   VTS_XX_0.VOB     (title)
 *
 * TITLE_VOBS      : VTS_XX_[1-9].VOB (title)
 *                   All files in the title set are opened and read as a single
 *                   file.
 */
typedef enum {
    DVD_READ_INFO_FILE,
    DVD_READ_INFO_BACKUP_FILE,
    DVD_READ_MENU_VOBS,
    DVD_READ_TITLE_VOBS
} dvd_read_domain_t;

/**
 * Opens a file on the DVD given the title number and domain.  If the title
 * number is 0, the video manager information is opened
 * (VIDEO_TS.[IFO,BUP,VOB]).  Returns a file structure which may be used for
 * reads, or 0 if the file was not found.
 */
dvd_file_t *DVDOpenFile( dvd_reader_t *dvd, int titlenum, 
			 dvd_read_domain_t domain );

/**
 * Closes a file and frees the associated structure.
 */
void DVDCloseFile( dvd_file_t *dvd_file );

/**
 * Reads the requested number of blocks from the file at the given offset.
 * Returns number of bytes read on success, -1 on error.  This call is only for
 * reading VOB data, and should not be used when reading the IFO files.  When
 * reading from an encrypted drive, blocks are decrypted using libcss where
 * required.
 */
int DVDReadBlocks( dvd_file_t *dvd_file, size_t offset,
                   size_t block_count, unsigned char *data );

/**
 * Seek to the given position in the file.  Returns the resulting position in
 * bytes from the beginning of the file.  The seek position is only used for
 * byte reads from the file, the block read call always reads from the given
 * offset.
 */
off64_t DVDFileSeek( dvd_file_t *dvd_file, off64_t offset );

/**
 * Reads the given number of bytes from the file.  This call can only be used
 * on the information files, and may not be used for reading from a VOB.  This
 * reads from and increments the currrent seek position for the file.
 */
int DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size );

#ifdef __cplusplus
};
#endif
#endif /* DVD_READER_H_INCLUDED */