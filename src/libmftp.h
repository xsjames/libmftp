/*   libmftp
 *
 *   Copyright (c) 2014 nkreipke
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */


#ifndef libmftp_libmftp_h
#define libmftp_libmftp_h

#include "ftpdefinitions.h"


//////////////////////
// VERBOSITY LEVELS //
//////////////////////

/* Log all messages from the server: */
#define FTP_SERVER_VERBOSE
/* Log raw content listing output: */
#define FTP_CONTENTLISTING_VERBOSE
/* Log all debug messages from libmftp: */
#define FTP_VERBOSE


//////////////
// SETTINGS //
//////////////

/* ENABLE FTP/TLS (a working OpenSSL installation is required): */
#define FTP_TLS_ENABLED


///////////////
// CONSTANTS //
///////////////

/* ftp_status Return Values: */
#define FTP_OK               0
#define FTP_ERROR          (-1)

/* ftp_activity Values: */
#define FTP_READ             1
#define FTP_WRITE            2

/* Use as startpos parameter for ftp_fopen to append to an existing remote file: */
#define FTP_APPEND        (~(0L))


////////////////////
// FTP_CONNECTION //
////////////////////

typedef struct _ftp_connection {
	/* Status of the connection. */
	ftp_status status;

	/* The current remote directory.
	 * Call ftp_reload_cur_directory first. */
	char * cur_directory;

	/* The connection timeout when waiting for a server answer. (60 by default) */
	unsigned long timeout;

	/* The status number of the latest server answer. */
	int last_signal;

	/* Error ID */
	int error;

	/* Sets whether a second connection should automatically be used for file transfers. (true by default)
	 * This is necessary for background file transfers.
	 * (Ignored if multiple connections are not allowed) */
	ftp_bool file_transfer_second_connection:1;

	/* Filters ".", ".." and other items that are neither files nor directories. */
	ftp_bool content_listing_filter:1;


	/* Internal */
	int _port;
	int _adr_fam;
	int _sockfd;
	int _data_connection;
	struct ftp_features __features;
	struct ftp_features * _current_features;
	int _last_answer_lock_signal;
	void * _last_answer_buffer;
	char *_host;
	char * _dataBuf;
	unsigned long _dataPointer;
	pthread_t _input_thread;
	int _input_trigger_signals[FTP_TRIGGER_MAX];
	struct timeval _wait_start;
	char *_mc_user, *_mc_pass;
	struct _ftp_connection *_parent, *_child;
	ftp_transfer_type _transfer_type;
	ftp_bool _internal_error_signal:1;
	ftp_bool _mc_enabled:1;
	ftp_bool _temporary:1;
	ftp_bool _termination_signal:1;
	ftp_bool _release_input_thread:1;
	ftp_bool _disable_input_thread:1;
#ifdef FTP_SERVER_VERBOSE
	void *verbose_command_buffer;
#endif
#ifdef FTP_TLS_ENABLED
	void *_tls_info;
	void *_tls_info_dc;
#endif
} ftp_connection;

typedef enum {
	/* Do not establish secure connection. */
	ftp_security_none
#ifdef FTP_TLS_ENABLED
	/* A secure TLS connection will be established if the server supports it. */
	,ftp_security_auto,
	/* Always establish a TLS connection. If the server does not support TLS,
	 * the connection will fail. */
	ftp_security_always
#endif
} ftp_security;

/*
 * When working with files, always check *(file->error) instead of the error variable in
 * the connection, as ftp_fopen may automatically establish new connections as needed.
 * file->error will always point to the correct error variable.
 */
typedef struct {
	ftp_connection *c;
	ftp_connection *parent;
	ftp_activity activity;

	ftp_bool eof;
	int *error;
} ftp_file;

typedef struct {
	unsigned int year, month, day, hour, minute, second;
} ftp_date;

typedef enum  {
	ft_file, ft_dir, ft_other
} ftp_file_type;

typedef struct {
	struct {
		ftp_bool size:1;
		ftp_bool modify:1;
		ftp_bool create:1;
		ftp_bool type:1;
		ftp_bool unixgroup:1;
		ftp_bool unixmode:1;
	} given;

	unsigned long size;
	ftp_date modify;
	ftp_date create;
	ftp_file_type type;
	unsigned int unixgroup;
	unsigned int unixmode;
} ftp_file_facts;

typedef struct _ftpcontentlisting {
	char *filename;
	ftp_file_facts facts;

	struct _ftpcontentlisting *next;
} ftp_content_listing;

/*
 * This contains error information only if ftp_open fails. Otherwise, the information
 * will be located in ftp_connection->error or *(ftp_file->error).
 */
extern int ftp_error;

FTP_I_BEGIN_DECLS


///////////////////
// FTP FUNCTIONS //
///////////////////

/* Open an ftp connection: ftp_open(host, port, security) */
ftp_connection *ftp_open(char *, unsigned int, ftp_security);

/* Close an ftp connection: ftp_close(ftpConnection) */
void ftp_close(ftp_connection *);

/* Authenticate: ftp_auth(ftpConnection, user, pass, allow_multiple_connections) */
ftp_status ftp_auth(ftp_connection *, char *, char *, ftp_bool);
/* If allow_multiple_connections = 1, username and password will be saved and used to
 * establish connections when trying to upload/download multiple files
 * simultaneously. */

/* Reload current directory: ftp_reload_cur_directory(ftpConnection) */
ftp_status ftp_reload_cur_directory(ftp_connection *);

/* Change current directory: ftp_change_cur_directory(ftpConnection, char *) */
ftp_status ftp_change_cur_directory(ftp_connection *, char *);

/* Get contents of current directory: ftp_contents_of_directory(ftpConnection, &items_count) */
ftp_content_listing *ftp_contents_of_directory(ftp_connection *, int *);
/* Free content listing: */
void ftp_free(ftp_content_listing *);

/* Check whether item exists in content listing:
 * ftp_item_exists_in_content_listing(ftpContentListing, filename, &item) */
ftp_bool ftp_item_exists_in_content_listing(ftp_content_listing *, char *, ftp_content_listing **);
/* item will be set to the content listing entry matching filename. */

/* Check whether item exists at current path:
 * ftp_item_exists(ftpConnection, filename, &item) */
#define ftp_item_exists(conn,filenm,item) ftp_item_exists_in_content_listing(ftp_contents_of_directory(conn,NULL),filenm,item)
/* item will be set to a content listing entry matching filename.
 * Check connection->error after using this function. */

/* Get size in bytes of remote file: ftp_size(ftpConnection, filename, &size) */
ftp_status ftp_size(ftp_connection *, char *, size_t *);

/* Opens a read/write stream to a file on the server: ftp_fopen(ftpConnection, filename, activity, startpos) */
ftp_file *ftp_fopen(ftp_connection *, char *, ftp_activity, unsigned long);
/* activity can be FTP_READ or FTP_WRITE.
 * Set startpos to FTP_APPEND to append the data to the remote file (write mode).
 * Keep in mind that many servers do not support values for startpos other than 0 when in write mode. */

/* Write into or read from file stream. Usage is very similar to fread and fwrite in stdio.
 *     ftp_fread(ptr, size, count, file)
 *     ftp_fwrite(ptr, size, count, file) */
size_t ftp_fread(void *, size_t, size_t, ftp_file *);
size_t ftp_fwrite(const void *, size_t, size_t, ftp_file *);
/* Write string: */
#define ftp_fwrites(string,file) ftp_fwrite(string,sizeof(char),strlen(string),file)
/* Check *(file->error) after using these functions.
 * When using ftp_fread, also check ftp_feof to determine the file end. */

/* Check whether the file is read completely: ftp_feof(ftpConnection) */
#define ftp_feof(file) (file->eof)

/* Closes a read/write stream. */
void ftp_fclose(ftp_file *);

/* Delete a file or an (empty) folder: ftp_delete(ftpConnection, filename, is_folder) */
ftp_status ftp_delete(ftp_connection *, char *, ftp_bool);

/* Rename a file on the server: ftp_rename(ftpConnection, current_filename, new_filename) */
ftp_status ftp_rename(ftp_connection *, char *, char *);

/* Move a file on the server: ftp_move(ftpConnection, filename, destination) */
#define ftp_move(conn,filenm,dest) ftp_rename(conn,filenm,dest)
/* destination must include the file name! */

/* Change mode of a file: ftp_chmod(ftpConnection, filename, mode) */
ftp_status ftp_chmod(ftp_connection *, char *, unsigned int);
/* This works only on UNIX servers! */

/* Create a folder: ftp_create_folder(ftpConnection, foldername) */
ftp_status ftp_create_folder(ftp_connection *, char *);

/* Send a NOOP command to the server: ftp_noop(ftpConnection, wait_for_response) */
ftp_status ftp_noop(ftp_connection *, ftp_bool);


FTP_I_END_DECLS

#endif
