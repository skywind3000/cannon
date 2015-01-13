/**********************************************************************
 *
 * icstream.h - definition of the basic stream interface
 * created on Feb. 4 2002 by skywind <skywind3000@hotmail.com>
 *
 * definition of data stream class in ansi c with the interfaces:
 * 
 * ISTREAM::reader - read data from the stream into the user buffer
 * ISTREAM::writer - write some data into the stream
 * ISTREAM::closer - close the stream
 * ISTREAM::seeker - move the data pointer and return the current pos
 *
 * the history of this file:
 * Feb. 4 2002   skywind   created including only ISTREAM struct
 *
 * aim to provide data independence stream operation inteface. all 
 * the sources are writen in ansi c. for more information please see 
 * the code with comment below.
 *
 **********************************************************************/

#ifndef __I_CSTREAM_H__
#define __I_CSTREAM_H__



/**********************************************************************
 * ISTREAM - definition of the basic stream interface
 **********************************************************************/
#define ISEEK_START   0   /* the offset is set to offset bytes */
#define ISEEK_CURRENT 1   /* to its current location plus offset bytes */
#define ISEEK_END     2   /* to the size of the stream plus offset bytes */

#define ISTREAM_READ(s,d,l)  (((s)->reader)(s,d,l))    /* read method  */
#define ISTREAM_WRITE(s,d,l) (((s)->writer)(s,d,l))    /* write method */
#define ISTREAM_SEEK(s,p,m)  (((s)->seeker)(s,p,m))    /* seek method  */
#define ISTREAM_CLOSE(s)     (((s)->closer)(s))        /* close method */

#define IEAGAIND  (-3)   /* error: try again same as would-block */
#define IEBROKEN  (-4)   /* error: the stream is broken */
#define IEIO      (-5)   /* error: an I/O error occurred while reading */
#define IESEEK    (-6)   /* error: the stream is not seekable */
    
#define ISTREAM_CANREAD     1   /* mode: the stream is readable */
#define ISTREAM_CANWRITE    2   /* mode: the stream is writable */
#define ISTREAM_CANSEEK     4   /* mode: the stream is seekable */

#ifndef _isize_t_defined
#define _isize_t_defined
typedef long isize_t;
#endif

/**********************************************************************
 * ISTREAM - definition of the basic stream interface
 **********************************************************************/
struct ISTREAM
{
    /********************** private interface ***********************/

    /*
     * reader - read data from the stream into the user buffer
     * returns how many bytes you really read, zero for EOF,
     * below zero for error (IEAGAIN/IEBROKEN/IEIO)
     */
    isize_t (*reader)(struct ISTREAM *stream, void *buffer, isize_t size);

    /*
     * writer - write some data into the stream
     * returns how many bytes you really wrote, blow zero for error
     */
    isize_t (*writer)(struct ISTREAM *stream, const void *buffer, isize_t size);

    /*
     * seeker - move the data pointer and return the current pos.
     * returns the current posistion after moving, blow zero for error
     */
    isize_t (*seeker)(struct ISTREAM *stream, isize_t position, int mode);
    
    /*
     * closer - close the stream, returns 0 for successful and others
     * for error (IEBROKEN, IEIO)
     */
    int (*closer)(struct ISTREAM *stream);

    /*********************** custom variable ************************/

    isize_t _totald;        /* custom: total size of the stream     */
    isize_t _offset;        /* custom: current offset of the stream */
    long _datamod;          /* custom: data mode of the stream      */
    long _datausr;          /* custom: addition data for user       */
    long _lasterr;          /* custom: last error number            */

    /************************ stream source *************************/

    /*
     * stream - user define data, reserved for save the real stream data
     * such as a FILE struct a pointer or something else.
     */
    void *stream;           /* stream of the current data source    */
};



#endif



