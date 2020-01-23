#ifndef MD5_H
# define MD5_H

# ifdef __alpha
typedef unsigned int uint32;
# else
typedef unsigned long uint32;
# endif

struct MD5Context
{

	uint32 buf[4];

	uint32 bits[2];

	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
			   unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;
# define md5_init MD5Init
# define md5_state_t MD5_CTX
# define md5_append MD5Update
# define md5_finish MD5Final

/*****************************************************************************/
/* modified by the giFT project */

char *md5_checksum (char *path, unsigned long size);

/*****************************************************************************/

#endif /* !MD5_H */

