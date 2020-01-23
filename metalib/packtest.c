#include <stdio.h>
#include <stdlib.h>

int
pack_integer (unsigned long value, unsigned char *buf, unsigned char *end)
{
	unsigned long i=value;
	unsigned char *ptr=buf;
	int len=1;
	printf("value=%d\n",value);
	while (i>127)
	{
		len++;
		i>>=7;
	}

	ptr+=len;
	if (ptr>end)
		return NULL;
	
	while (value>127)
	{
		*(--ptr)=(value &0x7f)|0x80;
		value>>=7;
	}
	*(--ptr)=value |0x80;
	ptr[len-1]&=0x7f;


	return len;
}

int main(void) {
	unsigned char foo[30];
	int e;
	e=pack_integer(0xffffff, foo, foo+8);
	printf("%d, %02x %02x %02x %02x %02x\n", e, foo[0],foo[1],foo[2],foo[3],foo[4]);
}
