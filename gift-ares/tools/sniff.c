/*
 * $Id: sniff.c,v 1.11 2006/01/08 03:12:56 mkern Exp $
 *
 * Based on printall.c from libnids/samples, which is
 * copyright (c) 1999 Rafal Wojtczuk <nergal@avet.com.pl>. All rights reserved.
*/

/* 
 * To run (as root):
 * ./sniff [interface]
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <zlib.h>

#include <nids.h>

extern char *nids_warnings[];

enum {
	STATE_CLIENT_KEY,
	STATE_ESTABLISHED,
	STATE_UNSUPPORTED,
	STATE_HTTP,
	STATE_PUSH,
	STATE_TRANSFER
};

static int id=0;
static time_t last_time=0;

struct session {
	unsigned short enc_state_16;
	unsigned char enc_state_8;
	int state;
	int id;
	int push;
	int encrypted;
};

#define INT(x) ntohl(*((unsigned long*)(data+x)));

#define int_ntoa(x)	inet_ntoa(*((struct in_addr *)&x))

char *
adres (struct tuple4 *addr, int id)
{
  static char buf[256];
  strcpy (buf, int_ntoa (addr->saddr));
  sprintf (buf + strlen (buf), ":%d -> ", addr->source);
  strcat (buf, int_ntoa (addr->daddr));
  sprintf (buf + strlen (buf), ":%d", addr->dest);
  if (id)
	  sprintf (buf + strlen (buf), " [%d]", id);
  return buf;
}

void print_bin_data(unsigned char * data, int len)
{
        int i;
        int i2;
        int i2_end;

//      printf("data len %d\n", data_len);

        for (i2 = 0; i2 < len; i2 = i2 + 16)
        {
                i2_end = (i2 + 16 > len) ? len: i2 + 16;
                for (i = i2; i < i2_end; i++)
                        if (isprint(data[i]))
                                fprintf(stderr, "%c", data[i]);
                        else
                        fprintf(stderr, ".");
                for ( i = i2_end ; i < i2 + 16; i++)
                        fprintf(stderr, " ");
                fprintf(stderr, " | ");
                for (i = i2; i < i2_end; i++)
                        fprintf(stderr, "%02x ", data[i]);
                fprintf(stderr, "\n");
        }
}









unsigned short table_1[256] =
{
	0x0000, 0x0808, 0xDC6C, 0x33DC, 0x45DD, 0xABF0, 0x5195, 0x296B, 0x5F4A,
	0x6CF8, 0x14FE, 0x798B, 0x1210, 0xD741, 0x0F4A, 0x4B15, 0xEAD2, 0x5E2E,
	0xC64F, 0x53F2, 0xB29A, 0xD81A, 0xB7CD, 0x4E7F, 0x29A1, 0x5459, 0x774C,
	0x3F24, 0xD35E, 0x476E, 0x7B54, 0x2630, 0xDFD3, 0x498B, 0xC5D2, 0xF9F8,
	0x7E15, 0xE34E, 0xD3C7, 0x0532, 0x241C, 0x24BC, 0x8034, 0x058C, 0x97C9,
	0x0278, 0xC643, 0xA690, 0xC53D, 0xB545, 0x8EB8, 0x34BE, 0xAE5A, 0x97C4,
	0xF497, 0xA4E0, 0xFF94, 0x3E68, 0xAD18, 0x4BB2, 0x15FC, 0xC5B6, 0x7ECA,
	0xE1B5, 0x828C, 0x92C4, 0xF403, 0xAF50, 0x56B5, 0x01F2, 0xB491, 0xFD2A,
	0xC627, 0xBFE3, 0x329C, 0x24C5, 0xE9E0, 0xC8C0, 0x96DA, 0xDD5C, 0xB079,
	0x49AF, 0x17F8, 0xA100, 0x47AF, 0xE08E, 0x2946, 0x45EF, 0x8B2E, 0xF5F5,
	0x2CA2, 0x2835, 0x44A3, 0x2D6D, 0x93A8, 0x7F86, 0x4901, 0x27EB, 0x9C18,
	0xBD5B, 0xBF13, 0x1491, 0x138A, 0x0F4D, 0x1B45, 0xCBA6, 0xCE4D, 0x61B0,
	0x9CAD, 0x6FD3, 0xBDA6, 0x7E71, 0x575E, 0xD1A1, 0xD128, 0x2432, 0x17EF,
	0x1131, 0x0F6F, 0x31A4, 0xAC46, 0x3A3B, 0x9202, 0x4125, 0x5DAB, 0xD64A,
	0x5087, 0x509B, 0xA80A, 0xB137, 0xE729, 0x5192, 0x7A8A, 0x4BE5, 0xC448,
	0xBD92, 0x8D4D, 0xB93C, 0x6961, 0x8AC3, 0x9887, 0xE169, 0xAB3E, 0x1CC5,
	0x78C2, 0x6755, 0x1D60, 0x0DCB, 0x8C71, 0xAC64, 0x14AA, 0xEBF6, 0xD6B4,
	0xAE70, 0xED4F, 0x46FA, 0xB36C, 0xBC06, 0x7FFF, 0xA9EC, 0x2881, 0x3AE1,
	0xF24E, 0x5D6B, 0x206F, 0xFCA5, 0x4E66, 0x0DF1, 0x1A18, 0xDDDE, 0x8DEF,
	0xB278, 0x19C6, 0xAB39, 0x043C, 0x4F51, 0xE77F, 0x8903, 0x53B8, 0x7142,
	0xE68F, 0x58E0, 0x5A90, 0x7CE0, 0x1251, 0xD9CD, 0xC2A2, 0x2E2D, 0xEC3E,
	0x7E59, 0x20A8, 0xE471, 0x673A, 0x3521, 0xCE9E, 0x965F, 0x1C1C, 0x9F8A,
	0xED7A, 0x8A62, 0x537C, 0x72C4, 0x3A0E, 0x2F46, 0xB0C2, 0xFEFC, 0xA136,
	0xB10D, 0x8A6C, 0x18E2, 0xBF47, 0x2611, 0x3BA1, 0xE15C, 0xB6B9, 0x7057,
	0xFAF3, 0x278A, 0x0A14, 0x2F34, 0x027B, 0x60FA, 0x6519, 0x5D23, 0x1511,
	0x742E, 0x8133, 0x75E6, 0xACE8, 0xD14C, 0x911E, 0x40DD, 0x5F53, 0x9525,
	0xDFD7, 0x9BA4, 0x41D4, 0xB26A, 0x8EC5, 0xE0F2, 0x5C62, 0x5D2B, 0x9F26,
	0xC7AE, 0x74FD, 0x3491, 0xB2BD, 0xD653, 0xC075, 0xE6DD, 0x2FBB, 0xC688,
	0x4478, 0x4BA2, 0xB3EB, 0x74F9
};

unsigned short table_2[256] = 
{
	0xCBA6, 0x1C3E, 0x60AE, 0x384F, 0x6800, 0xB1B7, 0x946F, 0xCDCB, 0xFCB6,
	0xE274, 0xDBB8, 0x50F5, 0x95FC, 0x3EDB, 0x69BA, 0x67B5, 0x6F11, 0xA3A4,
	0xD43D, 0x1DE6, 0x1DE5, 0x83E6, 0xB5DD, 0x007C, 0xCAED, 0x705E, 0x8280,
	0x33F2, 0x5144, 0x27FF, 0x0A1C, 0xF1F7, 0x6304, 0xBDC1, 0x65A9, 0x1A59,
	0x0DE4, 0x9FA3, 0x4A61, 0xC332, 0x3C13, 0x8B03, 0xFF40, 0xF453, 0x0D55,
	0xBA20, 0x70BA, 0xF9EF, 0x245D, 0xD6ED, 0x1EB7, 0xFA8A, 0x446B, 0x7F1A,
	0xE0D5, 0xBC22, 0xDB27, 0x655E, 0xBCDF, 0xC49D, 0x52BD, 0x5C07, 0x4B89,
	0x4173, 0x483B, 0xD4C6, 0x676F, 0x629D, 0x0229, 0xA1B5, 0x224E, 0x14BE,
	0xC368, 0x03AA, 0x3C83, 0x4691, 0xD64D, 0x51C1, 0x9AC2, 0x2594, 0x6BFE,
	0xC624, 0x7618, 0xFDF4, 0xAC0D, 0x3C1F, 0x4224, 0xA9B9, 0x9055, 0xEB62,
	0x8593, 0xBB34, 0x6912, 0x6C97, 0x20DB, 0xCEA6, 0x2545, 0x9325, 0xBF39,
	0xAF33, 0xBB47, 0xE843, 0x6DEE, 0x3B07, 0x4DAD, 0xD342, 0x95B6, 0x32C7,
	0xE85C, 0xBB14, 0xD28B, 0x603C, 0xF1EE, 0xAD22, 0x6599, 0xC03C, 0xAD45,
	0x554D, 0x3D82, 0x9BDA, 0x676F, 0x5343, 0xD0D4, 0xBCCF, 0x2DBA, 0x3905,
	0x46C9, 0x2928, 0x2818, 0xBEB6, 0xAA3F, 0x1232, 0xF3B7, 0xF7A7, 0x22F8,
	0xE2A4, 0x99D9, 0x94A2, 0x7114, 0x980D, 0xFEFB, 0x7F74, 0x26CC, 0xD37E,
	0xE624, 0x5BBE, 0x5C71, 0x4D79, 0xE48A, 0xB8FD, 0xDAA6, 0x991A, 0xE16C,
	0x01D8, 0xF6DB, 0x6285, 0xFE2D, 0x74AB, 0x9E0B, 0x9491, 0x02AE, 0xB152, //
	0x49B8, 0x11B4, 0xA9EF, 0xA839, 0xDB24, 0xEC2F, 0x6AE6, 0xF6A2, 0x98D2,
	0x497B, 0x99A0, 0xFC36, 0xCA3E, 0x5CF4, 0xFC97, 0x35D0, 0xADD9, 0x25C5,
	0x3254, 0xA986, 0xC549, 0xDC14, 0x8346, 0x6FFA, 0xB3E0, 0x3970, 0xFBE3,
	0xA6E0, 0x2B5B, 0x0879, 0xEAFE, 0x58D1, 0xA2DD, 0xA7CF, 0x2068, 0x2650,
	0xD42B, 0x6C3E, 0x87CB, 0xBC1A, 0x3B2B, 0x0E28, 0xFBC4, 0x62B4, 0x1398,
	0x8E34, 0x6D41, 0x3330, 0x9109, 0x3D36, 0xBB1B, 0xBD40, 0x4D22, 0xD35F,
	0xD5F4, 0x2A7F, 0x281A, 0xC2A6, 0x0C54, 0x2AFD, 0x176E, 0x3077, 0x9B00,
	0x3709, 0x9EE5, 0x429C, 0xDD9B, 0xF244, 0xEFC5, 0xAB6A, 0xBB81, 0xBBE3,
	0xEA55, 0xA12D, 0x2CE0, 0xB845, 0x7D94, 0xACD6, 0xF419, 0xDFB7, 0x2138,
	0xFBE5, 0xE757, 0xDE84, 0x65EA, 0x2192, 0x666F, 0xD241, 0xD7C2, 0x7340,
	0xD8F8, 0x3058, 0xAEFF, 0x6C24
};

unsigned short table_3[256] = 
{
	0x777F, 0x99EF, 0x65B7, 0x999E, 0x016E, 0xA28B, 0x413A, 0x4A42, 0x3FC3,
	0xB8A8, 0x89C6, 0x7FA9, 0xEBEA, 0x95CD, 0x2330, 0xC1F1, 0xD721, 0xDBA7,
	0xAF08, 0x56DB, 0xB3D9, 0x72D4, 0x9695, 0x63B3, 0x6F4D, 0xBAF0, 0x2B0F,
	0x7E87, 0xB762, 0x97DE, 0x181D, 0xDA86, 0x88BF, 0xA859, 0xA490, 0x7BB6,
	0xE12E, 0x31BC, 0x7D5E, 0x1DDB, 0xF233, 0x8D10, 0x06E3, 0xBAA4, 0x6FEB,
	0x2154, 0x7054, 0xDB19, 0xD828, 0x3712, 0x0584, 0x2043, 0xCDCB, 0x311F,
	0x7ADC, 0xAF1B, 0xE463, 0xF6B3, 0x2F96, 0x6BB5, 0x7805, 0x9AAB, 0xA23B,
	0x2FAE, 0x6D6B, 0x8C1B, 0x935D, 0x67D4, 0xF650, 0xDC18, 0x9296, 0xC516,
	0xB60C, 0x64C5, 0xF1C2, 0xECAC, 0xE26D, 0xDD9C, 0x5A79, 0x5388, 0x3CD6,
	0x2900, 0x81D1, 0xB0FE, 0xFF9C, 0x6701, 0xB653, 0xB4B0, 0x1B9E, 0x9970,
	0x443C, 0xEBBD, 0x6223, 0xE524, 0xEBF6, 0x012A, 0x76FA, 0xBC8C, 0xE6D7,
	0xC592, 0xA6CD, 0x7F76, 0x361A, 0xAA0B, 0xFDC6, 0x5C1F, 0x381C, 0x9A59,
	0x3A67, 0x7D7D, 0xBFD9, 0x6257, 0x78A4, 0xD2C9, 0x2AA6, 0x0AA4, 0xB142,
	0xFC51, 0x3031, 0xD88B, 0x6975, 0x297E, 0x68B8, 0xDD35, 0x2EB6, 0xF422,
	0xC587, 0x4012, 0xBAE3, 0x8504, 0x77B6, 0xB086, 0xDC9C, 0x8DAE, 0x0121,
	0xAAD2, 0x7FDA, 0xE377, 0x6BA8, 0x7C43, 0x72D1, 0xC7CD, 0xE2A9, 0x329E,
	0xC308, 0x29C6, 0x2ABC, 0xE2CC, 0xCEB7, 0x6CE8, 0xB3AF, 0xF2C3, 0x6662,
	0x8135, 0x40C3, 0x52B6, 0x9AB5, 0x587B, 0x6326, 0xD17E, 0x12A0, 0x04DC,
	0x42B1, 0xF849, 0x05B4, 0x0C99, 0x70E4, 0x1982, 0x36BE, 0xBF24, 0xC21F,
	0x7F01, 0x8AA2, 0x9766, 0x6924, 0xE375, 0x177B, 0xF74F, 0xD0CB, 0xA310,
	0xC7F2, 0xC89D, 0xF005, 0x4F72, 0xEEDC, 0xCEEE, 0x1414, 0x92D8, 0x6718,
	0xB709, 0x5D0A, 0x8E86, 0x1BA8, 0x2F6A, 0x6881, 0x2073, 0x140F, 0x960E,
	0xE69A, 0xBC35, 0x60EB, 0x077F, 0x1539, 0xC32D, 0x84D4, 0xDB20, 0x5D7F,
	0x87F4, 0x3576, 0xD8BA, 0x8F8A, 0x6906, 0x90EA, 0xB348, 0x786A, 0xAEAE,
	0x235B, 0x9500, 0xCE64, 0x1833, 0x58D7, 0xC8A4, 0xAF6B, 0x4441, 0x3773,
	0xCC09, 0x2DDD, 0x609B, 0x7DA4, 0x57E5, 0xA77B, 0xCD01, 0x80DA, 0x28DE,
	0xE2AD, 0x9D5D, 0x685E, 0x95D1, 0x2DF5, 0x2732, 0xA06F, 0x3F61, 0xFDB1,
	0x0647, 0xA199, 0x64C1, 0xC416, 0x4490, 0x6857, 0xA9C4, 0xFF6A, 0x915E,
	0x38EA, 0x5A23, 0xBA83, 0xB3C9
};


unsigned short calc_packet_key (unsigned char packet_seed,
                                unsigned short enc_state_16,
                                unsigned char enc_state_8)
{
	unsigned int ps = (unsigned int)packet_seed;
	unsigned int table_state;
//	unsigned int ebx;
	int i;

	table_state = (unsigned int) table_1[enc_state_8];
/*
	fprintf (stderr, "  calc_packet_key(): table_state: 0x%04x\n", (int)table_state);
	fprintf (stderr, "  calc_packet_key(): table_2[0x%02x]: 0x%04x\n", ps, (int)table_2[ps]);
	fprintf (stderr, "  calc_packet_key(): table_3[0x%02x]: 0x%04x\n", ps, (int)table_3[ps]);
*/
	for (i = 0; i < 4; i++)
	{
		table_state -= ps * 3;
		table_state += table_2[ps] - table_3[ps];
		table_state += enc_state_16;

/*
		ebx = table_2[ps] - table_3[ps];
		ebx += table_state - (ps * 3);  
		ebx += enc_state_16;            
		table_state = ebx;
*/
//		fprintf (stderr, "  calc_packet_key(): table_state: 0x%04x\n", (int)table_state);
	}

	return (table_state & 0xFFFF);
}

void decrypt_packet (unsigned char *data, int len, unsigned short key)
{
	unsigned char decoded;
	int i;

	for (i = 0; i < len; i++)
	{
		decoded = data[i] ^ (key >> 8);
		key = (key + data[i]) * 0xCE6D + 0x58BF;

		data[i] = decoded;
	}
}	

static void munge (unsigned char *data, int len, unsigned short key,
                   unsigned short mul, unsigned short add)
{
	int i;

	for (i = 0; i < len; i++)
	{
		data[i] = data[i] ^ (key >> 8);
		key = (key + data[i]) * mul + add;
	}
}

static void unmunge (unsigned char *data, int len, unsigned short key,
                     unsigned short mul, unsigned short add)
{
	unsigned char c;
	int i;

	for (i = 0; i < len; i++)
	{
		c = data[i] ^ (key >> 8);
		key = (key + data[i]) * mul + add;
		data[i] = c;
	}
}

/*
 * Decrypts packet type 0x33 which contains encryption state. The key is the
 * listening port the connection was made to.
 */
void decrypt_handshake_packet (unsigned char *data, int len, unsigned short key)
{
	unmunge (data, len, key, 0x5CA0, 0x15EC);
}	



int verify_port (int p)
{
	return !(p==1217 ||
		 p==1216 ||
		 p==1215 ||
		 p==6346 ||
		 (p<1000 && p!=80));
}

int verify_ip (struct tuple4 *addr)
{
#if 0
        unsigned long host = inet_addr ("192.168.0.222");
	
        return (addr->saddr == host ||
	        addr->daddr == host);
#else
	return 1;
#endif
}

void tcp_callback (struct tcp_stream *tcp, struct session **conn)
{
	char buf[32];

	struct session *c=*conn;

	switch (tcp->nids_state) {
	case NIDS_JUST_EST:
	{
		if (!verify_port (tcp->addr.dest) || !verify_port (tcp->addr.source))
			return;

		if (!verify_ip (&tcp->addr))
			return;

		c=malloc(sizeof(struct session));
		if (!c)
			abort();

		c->state=STATE_CLIENT_KEY;
		c->id=0;
		c->push=0;
		c->encrypted=0;
		c->enc_state_16=0;
		c->enc_state_8=0;
		
		*conn=c;
		
		tcp->client.collect++; // we want data received by a client
		tcp->server.collect++; // and by a server, too

//		fprintf (stderr, "%s established\n", buf);
		return;
	}
	case NIDS_CLOSE:
		// connection has been closed normally
		if (c && c->id) {
			fprintf (stderr, "[%d] closing\n", c->id);
			free(c);
			*conn=NULL;
		}
		return;
	case NIDS_RESET:
		// connection has been closed by RST
		if (c && c->id) {
			fprintf (stderr, "[%d] reset\n", c->id);
			free(c);
			*conn=NULL;
		}
		break;
	case NIDS_DATA:
	{
		// new data has arrived; gotta determine in what direction
		// and if it's urgent or not

		struct half_stream *hlf;
		int done=0;
		int len;
		unsigned char *data;
		int this_len;
		int read;
		int done_read=0;
		int server;

		if (tcp->client.count_new)
			hlf = &tcp->client; 
		else
			hlf = &tcp->server;
		server = !!tcp->client.count_new;

		len = hlf->count - hlf->offset;
		this_len=hlf->count_new;
		data=hlf->data;

		if (c->state != STATE_UNSUPPORTED) {
			time_t this_time=time(NULL);
			if (this_time-last_time>120) {
				/* print the time every 2 minutes */
				fprintf(stderr, "%s", ctime(&this_time));
				last_time=this_time;
			}
		}

		do {
			sprintf (buf, "[%d %s] ",c->id, server?"<-":"->");
			read=0;
		switch (c->state) {
		case STATE_CLIENT_KEY:
			fprintf(stderr, "%s\n", adres (&tcp->addr, c->id=++id));
			sprintf (buf, "[%d %s] ",c->id, server?"<-":"->");

			if (len>=5 && !server) {
				/* first check if this looks like a transfer */
				if (!memcmp(data, "GET ",4) ||
				     !memcmp(data, "HEAD ",5)) {
					/* ignore HTTP stuff on port 80 */
					if (tcp->addr.dest==80)
						c->state=STATE_UNSUPPORTED;
					else
						c->state=STATE_HTTP;
					break;
				}
				if (!memcmp(data, "PUSH ",5)) {
					c->state=STATE_PUSH;
					break;
				}
			}

			if (len>20 && !server) {
				/* binary transfer? */
				unsigned char tmpbuf[512];
				int skip;
				int llen=(len > 512) ? 512 : len;
				memcpy (tmpbuf, data, llen);
				unmunge (tmpbuf, llen, 0x5d1c, 0x5ca0, 0x15ec);
				skip = tmpbuf[2];
				if (len >= skip + 5) {
					//fprintf(stderr, "%s maybe binary? llen=%d, skip=%d, strlen=%d len=%d\n", buf, llen, skip, tmpbuf[skip+3]+tmpbuf[skip+4]*256, len-skip-5);				
					if (tmpbuf[skip+3]+tmpbuf[skip+4]*256 == len-skip-5) {
						c->state=STATE_TRANSFER;
						break;
					}
				}
			}

			if (len>=3 && server) {
				int type, plen;
				plen=data[0]+(data[1]<<8);
				type=data[2];
				if (len >= plen+2) {
					if (type==0x3B) {
						/* supernode is busy */
						fprintf(stderr, "%s got 0x3B (PACKET_ACK_BUSY), len %d, port %d\n",
							buf, plen, tcp->addr.dest);

						c->state=STATE_UNSUPPORTED;
						done=1;

					} else if (type==0x3C && plen >= 0x15) {
						decrypt_handshake_packet (data+3, plen, tcp->addr.dest);
						c->encrypted = 0;
						read=plen+3;
						c->state++;

						fprintf(stderr, "%s got 0x3C (PACKET_ACK_NOCRYPT), len %d, port %d\n",
							buf, plen, tcp->addr.dest);
						print_bin_data (data+3, plen);


					} else if ((type==0x33 || type == 0x38) && plen >= 0x15) {
						unsigned short tmp;
						unsigned char *packet_body;
						decrypt_handshake_packet (data+3, plen, tcp->addr.dest);
						/* extract encryption state */
						packet_body=data+3;
						tmp = packet_body[0];
						tmp |= ((unsigned short)packet_body[1]) << 8;
						
						//fprintf (stderr, "First word of 0x33 packet: 0x%04x\n", (int)tmp);
						
						c->enc_state_16 = packet_body[0x12] + (packet_body[0x13] << 8);
						c->enc_state_8 = packet_body[0x14];
						c->encrypted = 1;
						read=plen+3;
						c->state++;

						if(type == 0x33) {
							fprintf(stderr, "%s got 0x33 (PACKET_ACK), len %d, port %d, _16=0x%x, _8=0x%x, tmp=%d\n",
								buf, plen, tcp->addr.dest, c->enc_state_16, c->enc_state_8, tmp);
						} else if (type == 0x38) {
							fprintf(stderr, "%s got 0x38 (PACKET_ACK2), len %d, port %d, _16=0x%x, _8=0x%x, tmp=%d\n",
								buf, plen, tcp->addr.dest, c->enc_state_16, c->enc_state_8, tmp);

						} else {
							abort();
						}
						print_bin_data (data+3, plen);
					} else {
						c->state=STATE_UNSUPPORTED;
						done=1;
					}
				}
				else
					done=1;
			} else {
				if ((len >= 3) && ((data[0]+data[1]*256+3)==len)) {
					fprintf(stderr, "%s message type %02x, len %d [initial]\n", buf, data[2], len);
					print_bin_data(data+3,len-3);
					read=len;
					
				} else
					c->state=STATE_UNSUPPORTED;

				done=1;
			}

			break;

		case STATE_ESTABLISHED:
			if (len>=5) {
				int type, plen;
				int key;
				plen=data[0]+(data[1]<<8);
				type=data[2];
				if (plen >=2) {
					if (len >= plen+3) {
						if (type == 0x32) {
							unsigned char uncompressed[123456];
							unsigned long size=sizeof(uncompressed);
							int q;
							if ((q=uncompress (uncompressed, &size, data+3, plen))!=Z_OK)
								fprintf(stderr, "%s zlib decompression failed: %d", buf, q);
							else
							{
								fprintf(stderr, "%s message type %02x, len %d (compressed %d)\n", buf, type, size, plen);
								print_bin_data(uncompressed,size);
							}
						} else {
							/* FIXME: this gets executed for the initial client sent 0x5a packet
							 * which is sent before the encryption key is known and has a different
							 * format. No idea how to distinguish this case here. */
							
							if (c->encrypted) {
								key = calc_packet_key (data[3], c->enc_state_16, c->enc_state_8);
								decrypt_packet (data+5, plen-2, key);
							}
							
							fprintf(stderr, "%s message type %02x, len %d [%02x %02x]\n", buf, type, plen-2, data[3], data[4]);
							print_bin_data(data+5,plen-2);
						}
						read=plen+3;
					}
					else
						done=1;
				} else {
					fprintf(stderr, "%s bad plen %d", buf, plen);
					c->state=STATE_UNSUPPORTED;
					done=1;
				}
			} else
				done=1;
			break;
			
		case STATE_HTTP:
			if (!server) {
				fprintf(stderr, "%s HTTP request:\n", buf);
				fwrite(data, len, 1, stderr);
				read=len;
				done=1;
			} else {
				int i;
				for(i=0;i<len;i++) {
					if ((i<=len-4 && !memcmp(data+i, "\r\n\r\n", 4)) ||
					    (i<=len-2 && !memcmp(data+i, "\n\n", 2))) {
						fprintf(stderr, "%s HTTP reply:\n", buf);
						fwrite(data, i+2, 1, stderr);
						c->state=STATE_UNSUPPORTED; /* ignore the body */
						read=i+2;
						done=1;
						break;
					}
				}
				done=1;
			}	
			break;
		case STATE_PUSH:
			fprintf(stderr, "%s PUSH\n", buf);
			print_bin_data (data, len);
			c->push=1;
			read=len;
			done=1;
			c->state=STATE_HTTP;
			break;
		case STATE_TRANSFER:
			if (!server) {
				/* now we know for sure, we can scribble on the real data */
				int skip;
				unmunge (data, len, 0x5d1c, 0x5ca0, 0x15ec);
				skip = data[2]+5;
				unmunge (data+skip, len-skip, 0x3faa, 0xd7fb, 0x3efd);
				c->enc_state_16 = data[skip+5]+data[skip+6]*256;
				fprintf(stderr, "%s binary transfer [type %d], skip=%d, key=%02x\n", buf, data[skip], skip, c->enc_state_16);
				print_bin_data (data+skip, len-skip);
				{
				  unsigned char *ptr=data+skip+1, *end=data+len;
				  while (ptr+3<=end) {
				    int l=ptr[0]+ptr[1]*256, t=ptr[2];
				    fprintf(stderr, "type 0x%02x, len %d\n", t, l);
				    if (ptr+l+3<=end) {
					    switch (t) {
					    case 0x6:
						    unmunge (ptr+3, l, 0xb334, 0xce6D, 0x58bf);
						    break;
					    case 0xa:
						    unmunge (ptr+3, l, 0x5f40, 0x310f, 0x3a4e);
						    unmunge (ptr+3, l, 0x15d9, 0x5ab3, 0x8d1e);
						    if (l>9)
							    unmunge (ptr+12, l-9, 0xb334, 0xce6d, 0x58bf);
					    }
					    print_bin_data (ptr+3,l);
				    }
				    ptr+=l+3;
				  }
				}
				read=len;
				done=1;
			} else {
				if (len < 3 || len < data[2]+3) {
					done=1;
				} else {
					int skip;
					int i;
					unmunge (data, len, c->enc_state_16, 0xcb6f, 0x41ba);
					skip=data[2]+3;
					for(i=skip;i<len;i++) {
						if ((i<=len-4 && !memcmp(data+i, "\r\n\r\n", 4)) ||
						    (i<=len-2 && !memcmp(data+i, "\n\n", 2))) {
							fprintf(stderr, "%s encrypted HTTP reply (key=%02x skip=%d):\n", buf, c->enc_state_16, skip);
							fwrite(data+skip, i+2-skip, 1, stderr);
							c->state=STATE_UNSUPPORTED; /* ignore the body */
							read=i+2;
							done=1;
							break;
						}
					}
					if (i==len) {
					  fprintf(stderr, "%s encrypted HTTP reply? (key=%02x skip=%d):\n", buf, c->enc_state_16, skip);
					  print_bin_data(data+skip, len-skip);
					  read=len;
					  done=1;
					}
//					if (i==len)
//						fprintf(stderr, "[HTTP reply key=%02x skip=%d len=%d...],", c->enc_state_16, skip, len);

					done=1;
				}
			}
			break;
		default:
			/* fprintf(stderr, "%s %d skipped\n", buf,len); */
			done_read=-1;
			done=1;
		}
		this_len-=read;
		len-=read;
		data+=read;
		done_read+=read;
		if (len<0)
			fprintf(stderr, "WARNING: len=%d, this_len=%d\n", len, this_len);
		} while (!done && len>0);
		if (done_read>=0)
			nids_discard(tcp, done_read);

	}
	}
	return;
}

void udp_callback(struct tuple4 *addr, char *buf, int len, void* iph)
{
	unsigned char op, cmd;
	
	if (!verify_port (addr->dest) && !verify_port (addr->source))
		return;

	if (!verify_ip (addr))
		return;

	/* ignore dns */
	if (addr->dest == 53 || addr->source == 53)
		return;

	op = ((unsigned char*)buf)[0];
	cmd = ((unsigned char*)buf)[1];

	fprintf(stderr, "%s [UDP] (type 0x%02X, cmd 0x%02X, len %d)\n",
	        adres (addr, 0), op, cmd, len-2);

	switch (op)
	{
	case 0xE9: /* OP_DHT_HEADER */
		/* uncompressed payload */
		print_bin_data(buf+2, len-2);

		break;
	case 0xEA: /* OP_DHT_PACKEDPROT */
	{
		/* compress payload */
		unsigned char uncompressed[123456];
		unsigned long size=sizeof(uncompressed);
		int q;
		
		if ((q=uncompress (uncompressed, &size, buf+2, len-2))!=Z_OK)
		{
			fprintf(stderr, "zlib decompression failed: %d", q);
			break;
		}

		fprintf(stderr, "uncompressed len %d\n", size);
		print_bin_data(uncompressed,size);
		
		break;
	}
	default:
		return;
	}
}

void syslog (int type, int errnum, struct ip *iph, void *data)
{
	char saddr[20], daddr[20];
	struct tcphdr {
		u_int16_t th_sport;         /* source port */
		u_int16_t th_dport;         /* destination port */
	};

	if (errnum==9)
		return; /* ignore invalid TCP flags */

	switch (type) {
	case NIDS_WARN_IP:
        if (errnum != NIDS_WARN_IP_HDR) {
            strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
            strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
            fprintf(stderr,
                   "%s, packet (apparently) from %s to %s\n",
                   nids_warnings[errnum], saddr, daddr);
        } else
            fprintf(stderr, "%s\n",
                   nids_warnings[errnum]);
        break;

	case NIDS_WARN_TCP:
		strcpy(saddr, int_ntoa(iph->ip_src.s_addr));
		strcpy(daddr, int_ntoa(iph->ip_dst.s_addr));
		if (errnum != NIDS_WARN_TCP_HDR)
			fprintf(stderr,
				"%s,from %s:%hu to  %s:%hu\n", nids_warnings[errnum],
				saddr, ntohs(((struct tcphdr *) data)->th_sport), daddr,
				ntohs(((struct tcphdr *) data)->th_dport));
		else
			fprintf(stderr, "%s, from %s to %s\n",
				nids_warnings[errnum], saddr, daddr);
		break;
	}
}

int main (int argc, char **argv)
{
	if (argc>1)
		nids_params.device=argv[1];

	nids_params.n_tcp_streams=1<<18; /* default is *way* too small */
	nids_params.syslog=syslog;
	nids_params.scan_num_hosts=0;

	if (!nids_init ()) {
		fprintf(stderr,"%s\n",nids_errbuf);
		exit(1);
	}
	fprintf(stderr, "listening on %s\n", nids_params.device);
	nids_register_tcp (tcp_callback);
	nids_register_udp (udp_callback);
	nids_run ();
	return 0;
}
