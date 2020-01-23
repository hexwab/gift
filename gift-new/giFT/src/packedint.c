unsigned char *pack_integer (unsigned char *buf, int buflen, unsigned long value)
{
	unsigned long i;
	unsigned char *ptr=buf;
	int len=1;
	i=value;

	while (i>127)
	{
		len++;
		i>>=7;
	}

	if (len>buflen)
		return 0;
	
	ptr+=len;
	i=value;
	while (i>127)
	{
		*(--ptr)=(i &0x7f)|0x80;
		i>>=7;
	}

	*(--ptr)=i |0x80;
	ptr[len-1]&=0x7f;
	
	return ptr+len;
}

unsigned char *unpack_integer(unsigned char *buf, int buflen, unsigned long *value)
{
	unsigned long v=0;

        while (buflen-- > 0)
        {
                v <<= 7;
                v |= (*buf & 0x7f);
                if (!(*buf++ & 0x80))
                        break;
        }
	
        if (buflen < 0)
		return 0;
	
	if (value)
		*value = v;

	return buf;
}


#if 0

#include <stdio.h>
#include <assert.h>

int main(void)
{
	unsigned char buf[8];
	unsigned long i;
	
	for(i=0;;i++)
	{
		unsigned char *ptr = pack_integer (buf, 8, i);

		assert (ptr);
		
		{
			unsigned long j;
			unsigned char *ptr2;
			
			ptr2 = unpack_integer (buf, ptr-buf, &j);
			assert (ptr2==ptr);
			assert (i==j);
		}
	}
}
#endif
