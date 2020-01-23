/*
 * $Id: enc_type_20.c,v 1.5 2003/07/14 19:34:14 weinholt Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/*
 * This is the most recent pad mingling code for FastTrack as of 03/04/28
 * Used for encryption version 0x20
 */

typedef unsigned int u32;

/* our crude SEH replacement */

#define TRY(x) if((x) & 1) return;

/* some helper funcs */

#ifndef __GNUC__
#define __attribute__(x)
#endif

/* my_cos() and my_sin() are equal to cos()<0 and sin()<0. */
int __attribute__((const)) my_cos(unsigned char i)
{
	return (unsigned)((i * 39) % 245) - 62 < 122;
}

int __attribute__((const)) my_sin(unsigned char i)
{
	return (i * 46) % 289 > 144;
}

/* this is (unsigned int) floor(sqrt(((double)(((unsigned char)(i))))+1) + 0.001). */
int __attribute__((const)) my_sqrt(unsigned char i)
{
	int j, k;
	for (j=0,k=0;j++<=i;j+=++k<<1);
	return k;
}

#define ROR(value, count) ((value) >> ((count) & 0x1f) | ((value) << (32 - (((count) & 0x1f)))))
#define ROL(value, count) ((value) << ((count) & 0x1f) | ((value) >> (32 - (((count) & 0x1f)))))

#define ROREQ(value, count) value = ROR(value, count)
#define ROLEQ(value, count) value = ROL(value, count)

/* the entry point of this mess */
/* this all works on unsigned ints so endianess is not an issue */

void mix (u32 *key, u32 seed);

void enc_type_20 (u32 *key, u32 seed)
{
	mix (key, seed);
}

/* major functions which make calls to other funcs */

static void major_1 (u32 *key, u32 seed);
static void major_2 (u32 *key, u32 seed);
static void major_3 (u32 *key, u32 seed);
static void major_4 (u32 *key, u32 seed);
static void major_5 (u32 *key, u32 seed);
static void major_6 (u32 *key, u32 seed);
static void major_7 (u32 *key, u32 seed);
static void major_8 (u32 *key, u32 seed);
static void major_9 (u32 *key, u32 seed);
static void major_10 (u32 *key, u32 seed);
static void major_11 (u32 *key, u32 seed);
static void major_12 (u32 *key, u32 seed);
static void major_13 (u32 *key, u32 seed);
static void major_14 (u32 *key, u32 seed);
static void major_15 (u32 *key, u32 seed);
static void major_16 (u32 *key, u32 seed);
static void major_17 (u32 *key, u32 seed);
static void major_18 (u32 *key, u32 seed);
static void major_19 (u32 *key, u32 seed);
static void major_21 (u32 *key, u32 seed);
static void major_22 (u32 *key, u32 seed);
static void major_23 (u32 *key, u32 seed);
static void major_24 (u32 *key, u32 seed);
static void major_25 (u32 *key, u32 seed);

/* simple key manipulation functions */

static void minor_36 (u32 *key);
static void minor_37 (u32 *key);


/* minor_ implementation details below this line ;) */

#define minor_1(x) key[8] += my_sin(x&255) ? 0x4f0cf8d : x
#define minor_2(x) key[2] += key[2] < 0x36def3e1 ? key[2] : x
#define minor_3 key[10] ^= ROL(key[1], 20)
#define minor_4 key[16] -= key[6]
#define minor_5 key[10] -= key[9] * 0x55
#define minor_6 ROLEQ(key[0], key[19] ^ 0xc)
#define minor_7 key[17] += key[8] * 0xf6084c92
#define minor_8 key[12] ^= key[10] & 0x28acec82
#define minor_9(x) key[12] *= key[12] < 0x12d7bed ? key[12] : x
#define minor_10(x) key[18] += key[5] < 0xfd0aa3f ? key[5] : x
#define minor_12(x) key[11] &= my_cos(key[18]) ? 0x146a49cc : x
#define minor_13 key[2] &= my_cos(key[2]) ? 0x7ebbfde : key[11]
#define minor_17 key[19] ^= key[7] * 0x3a
#define minor_19 ROLEQ(key[6], ROR(key[8], 14))
#define minor_20 key[0] &= ROR(key[18], 31)
#define minor_22 ROREQ(key[3], key[11] ^ 0x7)
#define minor_26 key[0] |= my_cos(key[1]) ? 0x56e0e99 : key[8]
#define minor_27 key[18] += my_cos(key[15]) ? 0x10d11d00 : key[9]
#define minor_28 key[10] -= my_cos(key[15]) ? 0x268cca84 : key[9]
#define minor_29 key[3] -= my_cos(key[6]) ? 0x2031618a : key[8]
#define minor_30 ROLEQ(key[1], my_sin(key[5]) ? 4 : key[6])
#define minor_31(x) ROREQ(key[17], my_sin(key[6]) ? 29 : x)
#define minor_32(x) key[15] ^= my_sin(key[14]) ? 0x40a33fd4 : x
#define minor_34 key[7] ^= my_sqrt(key[11])
#define minor_35 key[5] += my_sqrt(key[7])

void minor_36 (u32 *key)
{
	key[3] ^= key[11] * 0xeef27425;
	key[3] += my_sqrt(key[0]);
	key[15] *= key[1] ^ 0xd89b4a;
	ROREQ(key[16], key[16] & 0x11);
	key[18] *= key[19] + 0xa0d8c0cf;
	key[7] *= key[0] < 0x6765080e ? key[0] : key[18];

	if(key[5] < 0xe848f43c)
		ROLEQ(key[9], key[5]);

	key[2] ^= key[5] < 0xa0d8c0cf ? key[5] : key[9] - 0xe848f43c;
	ROLEQ(key[12],  ROL(key[9] - 0xe848f43c, 11));
}

void minor_37 (u32 *key)
{
	ROLEQ(key[2], key[7] + 0x1b);
	key[2] ^= key[9] * 0x7941955;
	key[2] -= 0x796fa0af;
	key[3] *= my_sin(key[19]) ? 0x5ea67f83 : key[5];
	key[4] -= key[4] ^ 0x692c9ef9;
	key[10] += key[1] ^ 0xc43baf0b;
	key[12] *= key[7] - 0xc43baf0b;
	key[13] ^= 0xd;
	key[17] ^= key[17] - 0x1259dbb;
	ROREQ(key[17], 10);
	key[18] += key[0] ^ 0x3cf1856;
}

void major_1 (u32 *key, u32 seed)
{
	int type = (key[17] ^ key[4] ^ key[13]) % 13;

	if(type == 9)
	{
		key[7] |= 0x3e73450d;
		minor_31 (0x9);
		minor_36 (key);
	}

	key[11] &= key[19] & 0x170b54ed;

	if(type == 10)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		minor_27;
		major_23 (key, key[8]);
	}

	ROREQ(key[1], key[14] < 0x164d8d96 ? key[14] : key[4]);

	if(type == 12)
	{
		TRY(minor_1 (0xc0948cf0));
		minor_28;
		major_24 (key, key[18]);
	}

	if(type == 0)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		minor_31 (0x15);
		major_19 (key, key[12]);
	}

	ROLEQ(key[6], key[13] ^ 0x2);

	if(type == 6)
	{
		TRY(ROREQ(key[1], 0x4));
		key[9] ^= key[7] * 0x44;
		major_25 (key, seed);
	}

	seed += my_sin(seed) ? 0x160df35d : seed;

	if(type == 3)
	{
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		key[13] *= ROR(key[3], 5);
		major_17 (key, key[15]);
	}

	if(type == 0)
	{
		minor_19;
		key[13] *= ROR(key[3], 5);
		major_4 (key, key[8]);
	}

	seed &= key[19] | 0xe00682c6;

	if(type == 1)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		key[9] |= key[7] ^ 0x2a19119f;
		major_18 (key, key[12]);
	}

	key[16] += my_sin(seed) ? 0xe00682c6 : key[7];

	if(type == 2)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_15 (key, seed);
	}

	if(type == 7)
	{
		key[4] -= key[17] ^ 0x2217cf47;
		key[13] *= ROR(key[3], 5);
		major_3 (key, key[14]);
	}

	seed += key[15] ^ 0x1777bc26;

	if(type == 4)
	{
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		minor_34;
		major_21 (key, key[18]);
	}

	key[5] *= my_sqrt(key[9]);

	if(type == 11)
	{
		ROREQ(key[4], 0x2);
		minor_32 (0x8517ae30);
		major_16 (key, key[4]);
	}

	key[13] &= key[18] - 0xeb6dee4;

	if(type == 5)
	{
		minor_30;
		TRY(minor_3);
		minor_36 (key);
	}

	if(type == 8)
	{
		key[7] |= 0x7de964ed;
		TRY(minor_4);
		major_23 (key, key[3]);
	}
}

void major_2 (u32 *key, u32 seed)
{
	int type = key[10] & 15;

	if(type == 5)
	{
		minor_27;
		key[7] &= key[13] ^ 0x21aaf758;
		major_25 (key, key[0]);
	}

	key[0] -= seed * 0x36;

	if(type == 13)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[6] += 0xfe07af0e - key[3];
		major_17 (key, seed);
	}

	if(type == 12)
	{
		key[6] ^= 0x9374c368;
		key[7] &= 0xc45b99ee;
		major_4 (key, key[14]);
	}

	key[7] -= key[8] | 0x1a1a9407;

	if(type == 6)
	{
		ROREQ(key[9], 12);
		key[3] -= key[0] ^ 0x185f3b0d;
		major_18 (key, key[14]);
	}

	key[2] += key[0] + 0x19259d5;

	if(type == 8)
	{
		key[9] ^= key[7] * 0x44;
		key[2] ^= key[15] << 5;
		major_15 (key, seed);
	}

	if(type == 11)
	{
		minor_34;
		key[3] -= key[0] ^ 0x185f3b0d;
		major_3 (key, key[15]);
	}

	key[16] &= seed -0x1badcb5;

	if(type == 15)
	{
		key[14] |= key[3] ^ 0x4345732;
		ROREQ(key[4], 0x1);
		major_21 (key, key[3]);
	}

	key[5] -= my_cos(key[4]) ? 0xffcdb92f : key[14];

	if(type == 1)
	{
		key[13] -= key[1];
		key[7] |= 0x45e184c5;
		major_16 (key, key[9]);
		TRY(minor_1 (0x149a97a0));
		TRY(minor_10 (0xd87d888e));
		major_1 (key, key[9]);
	}

	key[5] *= key[8] + 0xffcdb92f;

	if(type == 4)
	{
		TRY(minor_10 (0x130aa218));
		key[13] *= ROR(key[3], 5);
		major_14 (key, key[6]);
	}

	ROLEQ(key[1], key[15] < 0xbdc3f45b ? key[15] : key[9]);

	if(type == 14)
	{
		minor_30;
		key[13] *= 0x7f0d5ead;
		major_6 (key, key[5]);
	}

	if(type == 0)
	{
		key[6] += key[19] - 0x3f5675d6;
		TRY(minor_5);
		major_9 (key, seed);
	}

	key[6] += key[3] * 0x79;

	if(type == 9)
	{
		key[9] &= 0x3eb4ed97;
		minor_30;
		major_25 (key, key[6]);
	}

	key[16] ^= my_cos(key[7]) ? 0x2d36f243 : key[13];

	if(type == 0)
	{
		key[0] += key[18] ^ 0x4ac16b8d;
		minor_28;
		major_17 (key, key[2]);
	}

	if(type == 7)
	{
		key[10] += 0x8958821;
		TRY(minor_1 (0x115e64d4));
		major_4 (key, key[19]);
	}

	key[14] &= key[3] ^ 0xb8eb772d;

	if(type == 10)
	{
		key[13] -= key[1];
		key[2] ^= key[15] << 5;
		major_18 (key, key[8]);
	}

	ROREQ(key[1], key[12] * 0x5);

	if(type == 3)
	{
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		major_15 (key, key[15]);
	}

	if(type == 2)
	{
		key[7] &= 0x5cf54b9a;
		key[13] *= 0xa02fe00;
		major_3 (key, key[14]);
	}

	key[12] ^= my_sin(key[0]) ? 0x96d5a5a4 : key[5];
}

void major_3 (u32 *key, u32 seed)
{
	int type = (key[5] ^ seed ^ key[12]) % 10;
	u32 a = 0x3074a456;

	a += 0xe7812af4;
	seed *= key[6] | 0x4723b25;

	if(type == 0)
	{
		minor_22;
		TRY(minor_5);
		minor_37 (key);
	}

	key[2] -= key[4] * 0xd;

	if(type == 5)
	{
		key[7] ^= 0x414517ea;
		minor_29;
		minor_36 (key);
	}

	a += 0xf3de60a6;
	seed += key[12] * 0x19;

	if(type == 1)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		minor_19;
		major_23 (key, seed);
	}

	seed += key[7] + a;

	if(type == 2)
	{
		minor_29;
		key[16] += 0x1f5b0c59;
		major_24 (key, seed);
	}

	a += 0x93fd548f;
	key[15] -= key[0] ^ 0x16bee8c4;

	if(type == 4)
	{
		TRY(minor_7);
		minor_28;
		major_19 (key, seed);
	}

	key[18] ^= key[11] + a;

	if(type == 6)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[6] += key[19] - 0x3f5675d6;
		major_25 (key, seed);
	}

	a += 0xe45f06fe;
	ROLEQ(key[14], key[19]);

	if(type == 8)
	{
		minor_30;
		key[12] += key[6] + 0x21d7bf61;
		major_17 (key, seed);
	}

	ROREQ(key[0], key[13] * 0x13);

	if(type == 9)
	{
		TRY(minor_2 (0x70da1d6f));
		minor_29;
		major_4 (key, seed);
	}

	a += 0xce7dcf3a;
	seed ^= key[16] < 0x33671de9 ? key[16] : key[17];

	if(type == 7)
	{
		minor_22;
		TRY(minor_3);
		major_18 (key, key[5]);
	}

	seed &= seed << 6;

	if(type == 3)
	{
		minor_17;
		key[2] ^= key[15] << 5;
		major_15 (key, key[19]);
	}
}

void major_4 (u32 *key, u32 seed)
{
	int type = key[6] % 7;
	u32 a = 0x775fd18;

	seed ^= ROL(key[3], 18);

	if(type == 6)
	{
		key[6] += key[19] - 0x3f5675d6;
		TRY(minor_5);
		minor_37 (key);
	}

	key[15] += seed * 0x32;
	a += 0xee31a212;
	key[5] += 0xc93495e4 - key[14];

	if(type == 2)
	{
		TRY(minor_10 (0x10db4a9d));
		key[6] += 0xfe07af0e - key[3];
		minor_36 (key);
	}

	key[12] *= my_cos(key[14]) ? a : key[17];

	if(type == 0)
	{
		minor_17;
		key[9] |= key[7] ^ 0x2a19119f;
		major_23 (key, key[8]);
	}

	a += 0xea83bbf0;
	key[6] &= key[7] | a;
	key[11] ^= my_cos(key[0]) ? 0x3a2c762b : seed;

	if(type == 4)
	{
		TRY(minor_3);
		TRY(ROREQ(key[1], 0x1c));
		major_24 (key, seed);
	}

	a += 0xedc3d0d6;
	key[3] -= my_sqrt(key[9]);

	if(type == 5)
	{
		key[6] ^= 0x47a791f;
		key[0] += key[18] ^ 0x4ac16b8d;
		major_19 (key, key[18]);
	}

	seed &= my_cos(key[7]) ? a : key[3];
	a += 0xc2688371;
	key[0] -= key[15] * 0x43;

	if(type == 1)
	{
		minor_19;
		key[6] ^= 0x424d4b7d;
		major_25 (key, key[3]);
	}

	key[1] -=  ROR(key[18], 19);
	a += 0xd993439c;
	key[17] ^= my_sin(key[14]) ? a : key[16];

	if(type == 0)
	{
		key[3] -= key[0] ^ 0x185f3b0d;
		key[2] *= key[3] + 0xd6863a6;
		major_17 (key, key[14]);
	}
}

void major_5 (u32 *key, u32 seed)
{
	int type = (key[13] ^ key[6] ^ key[16]) & 15;
	u32 a = 0xfe1d15;

	if(type == 7)
	{
		minor_20;
		key[9] += ROL(key[4], 9);
		major_17 (key, key[15]);
	}

	a += 0xdfa1459a;
	key[2] ^= key[15] - a;

	if(type == 15)
	{
		minor_31 (0x7);
		key[9] |= key[7] ^ 0x2a19119f;
		major_4 (key, key[10]);
	}

	if(type == 14)
	{
		key[9] ^= 0x19b844e;
		key[5] -= key[15];
		major_18 (key, seed);
	}

	key[5] += key[8] * 0x49;

	if(type == 4)
	{
		key[14] |= key[3] ^ 0x4345732;
		TRY(minor_8);
		major_15 (key, key[19]);
	}

	if(type == 1)
	{
		minor_27;
		minor_17;
		major_3 (key, key[4]);
	}

	a += 0xf5224fea;
	seed += key[16] < 0x4dfe57f8 ? key[16] : key[17];

	if(type == 2)
	{
		key[13] *= 0x7ae310dc;
		key[12] ^= key[15] - 0xf5cfde0;
		major_21 (key, key[11]);
		key[13] *= ROR(key[3], 5);
		minor_20;
		major_16 (key, key[10]);
	}

	key[5] += key[6] + a;

	if(type == 10)
	{
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		key[9] |= key[7] ^ 0x2a19119f;
		major_1 (key, key[10]);
	}

	if(type == 12)
	{
		key[16] += 0x203fdf50;
		minor_20;
		major_14 (key, key[8]);
	}

	a += 0xe4fb84fb;
	key[1] += my_sin(seed) ? a : seed;

	if(type == 6)
	{
		key[4] ^= 0xca8e79ab;
		key[13] -= key[1];
		major_6 (key, key[14]);
	}

	if(type == 3)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		key[2] ^= key[15] << 5;
		major_9 (key, seed);
	}

	key[3] |= my_cos(key[3]) ? a : key[17];

	if(type == 9)
	{
		minor_17;
		key[5] -= key[15];
		major_2 (key, seed);
	}

	if(type == 13)
	{
		ROREQ(key[4], 0x2);
		key[4] -= key[17] ^ 0x2217cf47;
		major_17 (key, seed);
	}

	a += 0xf79e94b9;
	seed ^= key[2] * a;

	if(type == 11)
	{
		key[19] += 0x12b9e29d - key[12];
		key[7] &= key[13] ^ 0x21aaf758;
		major_4 (key, key[17]);
	}

	if(type == 0)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		minor_31 (0x7);
		major_18 (key, seed);
	}

	key[8] += 0xf1030e9c - key[12];

	if(type == 5)
	{
		key[6] ^= 0xea99e155;
		key[19] += 0x12b9e29d - key[12];
		major_15 (key, seed);
	}

	if(type == 8)
	{
		key[7] &= 0x710c48e8;
		key[2] *= key[3] + 0xd6863a6;
		major_3 (key, key[17]);
	}

	a += 0xf697757a;
	key[15] += a - key[1];

	if(type == 0)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[3] -= key[0] ^ 0x185f3b0d;
		major_21 (key, seed);
	}

	if(type == 1)
	{
		TRY(minor_3);
		key[12] *= key[12];
		major_16 (key, key[12]);
	}

	key[6] *= key[5] * 0x1d;
}

void major_6 (u32 *key, u32 seed)
{
	int type = key[17] % 15;
	u32 a = 0x10572198;

	if(type == 0)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		key[13] *= 0x22dd951f;
		major_24 (key, key[8]);
	}

	a += 0xd1ae1094;
	key[11] -= my_sin(key[9]) ? a : key[7];

	if(type == 13)
	{
		TRY(ROREQ(key[1], 0x4));
		key[12] ^= key[15] - 0xf5cfde0;
		major_19 (key, key[0]);
	}

	key[10] -= key[6] ^ 0x1289de2;

	if(type == 8)
	{
		ROREQ(key[9], 10);
		TRY(minor_13);
		major_25 (key, key[4]);
	}

	if(type == 5)
	{
		key[13] *= 0x6a94c749;
		key[18] -=  key[13] ^ 0x154abcdf;
		major_17 (key, seed);
	}

	a += 0xe6300abd;
	ROLEQ(key[16], my_sqrt(key[17]));

	if(type == 2)
	{
		key[16] += 0x3f147441;
		major_4 (key, key[16]);
	}

	key[9] += my_sqrt(key[3]);

	if(type == 14)
	{
		ROREQ(key[9], 15);
		key[13] -= key[1];
		major_18 (key, seed);
	}

	a += 0xa4cd3449;
	seed = key[6] ^ seed ^ 0x202ab323;

	if(type == 9)
	{
		key[5] += key[0] ^ 0x3e17add3;
		key[4] -= key[17] ^ 0x2217cf47;
		major_15 (key, key[8]);
	}

	if(type == 6)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[6] += key[19] - 0x3f5675d6;
		major_3 (key, key[16]);
	}

	key[15] ^= my_sqrt(key[10]);

	if(type == 1)
	{
		TRY(minor_2 (0xb30d40d0));
		key[10] *= key[10] - 0x5eae6bf;
		major_21 (key, key[13]);
	}

	a += 0xe2773b9c;
	key[0] -= key[11] ^ 0x1284af29;

	if(type == 4)
	{
		key[5] += key[0] ^ 0x3e17add3;
		minor_29;
		major_16 (key, key[17]);
	}

	ROLEQ(seed, key[11] * 0x10);

	if(type == 11)
	{
		key[9] ^= 0x1d8f33a6;
		TRY(minor_9 (0x13ee15c3));
		major_1 (key, key[19]);
	}

	if(type == 0)
	{
		TRY(minor_3);
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_14 (key, key[16]);
	}

	a += 0xfaa9c69b;
	key[9] |= key[9] ^ 0x2ad7629;

	if(type == 10)
	{
		TRY(ROREQ(key[1], 0xc));
		TRY(minor_9 (0xe8869877));
		major_24 (key, seed);
	}

	key[4] *= key[12] * a;

	if(type == 12)
	{
		key[9] += ROL(key[4], 9);
		TRY(minor_7);
		major_19 (key, key[5]);
	}

	if(type == 7)
	{
		key[14] |= key[3] ^ 0x4345732;
		TRY(minor_9 (0xdd1ca541));
		major_25 (key, key[1]);
	}

	a += 0xd268da5f;
	seed *= key[4] + 0x76e5a087;

	if(type == 3)
	{
		TRY(minor_5);
		TRY(minor_1 (0x62f4d3c4));
		major_17 (key, seed);
	}
}

void major_7 (u32 *key, u32 seed)
{
	int type = (key[10] ^ key[11] ^ key[18]) & 15;
	u32 a = 0xc6ef5e80;

	if(type == 3)
	{
		minor_27;
		TRY(minor_2 (0x54bcde17));
		major_1 (key, key[14]);
	}

	if(type == 9)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		key[13] -= key[1];
		major_14 (key, key[12]);
	}

	key[8] |= a + key[1];

	if(type == 2)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[6] += 0xfe07af0e - key[3];
		major_6 (key, key[16]);
	}

	if(type == 5)
	{
		key[5] -= key[15];
		key[16] += 0x3fa3dc2f;
		major_9 (key, key[5]);
	}

	if(type == 1)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		key[14] |= key[3] ^ 0x4345732;
		major_2 (key, key[3]);
	}

	key[15] -= a + key[19];

	if(type == 4)
	{
		key[2] ^= key[15] << 5;
		minor_28;
		major_5 (key, key[7]);
	}

	if(type == 1)
	{
		key[0] += key[6] * 0x3c;
		minor_31 (0x15);
		major_12 (key, key[16]);
	}

	if(type == 9)
	{
		key[5] += key[0] ^ 0x3e17add3;
		key[9] &= 0x4bd89b02;
		major_11 (key, key[19]);
	}

	a += 0xfdc73e57;
	seed -= key[0] ^ 0x3b61016b;

	if(type == 4)
	{
		key[5] -= key[15];
		TRY(minor_9 (0x1984a749));
		major_13 (key, key[3]);
	}

	if(type == 3)
	{
		key[9] += ROL(key[4], 9);
		minor_28;
		major_22 (key, key[4]);
	}

	if(type == 7)
	{
		TRY(minor_3);
		minor_31 (0xd);
		major_8 (key, key[5]);
	}

	ROLEQ(key[11], key[10] ^ 0x1a);

	if(type == 8)
	{
		key[18] -=  key[13] ^ 0x154abcdf;
		key[19] += 0x12b9e29d - key[12];
		major_10 (key, seed);
	}

	if(type == 14)
	{
		key[19] += 0x12b9e29d - key[12];
		ROREQ(key[9], 3);
		major_1 (key, seed);
	}

	a += 0xfb777bc8;
	seed -= key[14] * a;

	if(type == 0)
	{
		key[6] ^= 0x94eaa20d;
		minor_27;
		major_14 (key, key[6]);
	}

	if(type == 13)
	{
		key[12] += 0x2ac57dfa;
		key[2] ^= key[15] << 5;
		major_6 (key, key[4]);
	}

	if(type == 8)
	{
		key[18] *= key[10] + 0x466e09cf;
		key[13] -= key[1];
		major_9 (key, seed);
	}

	key[4] += 0xa207344d - seed;

	if(type == 12)
	{
		TRY(minor_2 (0x80a1da17));
		key[13] *= 0xa02fe00;
		major_2 (key, key[3]);
	}

	if(type == 2)
	{
		key[13] *= 0x6aa5cc8c;
		key[6] ^= 0xaefb322;
		major_5 (key, key[9]);
	}

	if(type == 0)
	{
		minor_31 (0xb);
		key[5] += key[0] ^ 0x3e17add3;
		major_12 (key, key[5]);
	}

	a += 0xcb22b4b2;
	seed ^= key[18] ^ 0xe6830c9;

	if(type == 6)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		key[7] &= key[13] ^ 0x21aaf758;
		major_11 (key, seed);
	}

	if(type == 7)
	{
		key[3] -= key[0] ^ 0x185f3b0d;
		TRY(minor_2 (0xb11da063));
		major_13 (key, key[3]);
	}

	if(type == 15)
	{
		minor_34;
		key[4] ^= 0x41e634f6;
		major_22 (key, key[17]);
	}

	key[0] ^= my_sin(seed) ? a : key[8];

	if(type == 11)
	{
		minor_28;
		key[2] ^= key[15] << 5;
		major_8 (key, seed);
	}

	if(type == 5)
	{
		key[9] += ROL(key[4], 9);
		key[7] &= key[13] ^ 0x21aaf758;
		major_10 (key, key[16]);
	}

	a += 0xb011aa26;
	seed += key[1] * 0x3e;

	if(type == 6)
	{
		minor_34;
		ROREQ(key[4], 0x9);
		major_1 (key, key[12]);
	}

	if(type == 10)
	{
		minor_22;
		key[12] ^= key[15] - 0xf5cfde0;
		major_14 (key, seed);
	}

	key[1] ^= key[2] & a;
}

void major_8 (u32 *key, u32 seed)
{
	int type = (key[2] ^ seed ^ key[17]) & 15;
	u32 a = 0x16332817;

	if(type == 7)
	{
		key[13] -= key[1];
		minor_26;
		major_21 (key, seed);
	}

	if(type == 0)
	{
		minor_35;
		TRY(minor_2 (0xe0b52e33));
		major_16 (key, key[15]);
	}

	a += 0xcd45850f;
	seed -= ROR(key[2], a);

	if(type == 5)
	{
		key[4] ^= 0x9aa940f;
		key[5] += key[0] ^ 0x3e17add3;
		major_1 (key, key[12]);
	}

	if(type == 1)
	{
		minor_22;
		minor_17;
		major_14 (key, key[1]);
	}

	key[12] -= key[17] * 0x74;

	if(type == 1)
	{
		key[12] += key[6] + 0x21d7bf61;
		key[13] *= ROR(key[3], 5);
		major_6 (key, key[5]);
	}

	if(type == 14)
	{
		key[4] ^= 0x91ac407e;
		TRY(minor_7);
		major_9 (key, seed);
	}

	a += 0xe62a6fb0;
	key[3] ^= key[7] + 0x137c9f7d;

	if(type == 0)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		key[2] ^= key[15] << 5;
		major_2 (key, key[5]);
	}

	if(type == 4)
	{
		key[10] += 0x8958821;
		key[16] += 0x3d2948e4;
		major_5 (key, key[4]);
	}

	if(type == 3)
	{
		key[3] -= key[0] ^ 0x185f3b0d;
		key[9] |= key[7] ^ 0x2a19119f;
		major_12 (key, key[12]);
	}

	key[11] += key[17] < a ? key[17] : key[4];

	if(type == 4)
	{
		key[9] ^= 0x1c686298;
		key[12] ^= key[15] - 0xf5cfde0;
		major_11 (key, key[9]);
	}

	if(type == 13)
	{
		key[7] |= 0x378d3869;
		TRY(minor_13);
		major_13 (key, key[13]);
	}

	a += 0xf3f6d9ae;
	seed *= key[12] + a;

	if(type == 7)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_22 (key, seed);
	}

	if(type == 8)
	{
		key[13] *= 0x1dcee48b;
		minor_30;
		major_21 (key, seed);
	}

	key[10] += 0xaa3373fc - key[6];

	if(type == 5)
	{
		minor_31 (0xe);
		TRY(minor_1 (0xbc90d50));
		major_16 (key, seed);
	}

	if(type == 15)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		key[0] += key[18] ^ 0x4ac16b8d;
		major_1 (key, seed);
	}

	if(type == 6)
	{
		minor_34;
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		major_14 (key, seed);
	}

	a += 0xe743b2d6;
	seed ^= key[11] < a ? key[11] : key[2];

	if(type == 6)
	{
		key[16] += 0x1a36972b;
		minor_17;
		major_6 (key, key[5]);
	}

	if(type == 12)
	{
		TRY(minor_12 (0xb6571d3f));
		key[0] += key[6] * 0x3c;
		major_9 (key, key[9]);
	}

	seed *= key[14] + 0x9baa8db;

	if(type == 9)
	{
		TRY(minor_9 (0xe378a0ed));
		key[5] += key[0] ^ 0x3e17add3;
		major_2 (key, key[8]);
	}

	if(type == 11)
	{
		TRY(minor_10 (0xbd149bd9));
		minor_32 (0x6476f303);
		major_5 (key, seed);
	}

	a += 0xee69160e;
	key[17] += my_sqrt(key[12]);

	if(type == 10)
	{
		key[13] *= 0xa02fe00;
		key[12] += key[6] + 0x21d7bf61;
		major_12 (key, key[19]);
	}

	if(type == 3)
	{
		key[13] *= 0x111b84cd;
		minor_30;
		major_11 (key, key[2]);
	}

	key[7] ^= key[9] * 0x27219096;

	if(type == 2)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[16] += 0xfe49a900;
		major_13 (key, key[15]);
		TRY(minor_6);
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_22 (key, key[19]);
	}

	a += 0xd8c0d94b;
	ROLEQ(key[2], a ^ seed);
}

void major_9 (u32 *key, u32 seed)
{
	int type = key[8] & 15;
	u32 a = 0x3a6d8ff;

	if(type == 10)
	{
		minor_29;
		key[7] &= key[13] ^ 0x21aaf758;
		major_19 (key, key[0]);
	}

	a += 0xdf6fc3de;
	seed |= seed + 0x20029bc7;

	if(type == 3)
	{
		key[16] += 0x45e88961;
		TRY(minor_8);
		major_25 (key, key[15]);
	}

	if(type == 8)
	{
		TRY(minor_6);
		minor_22;
		major_17 (key, key[2]);
	}

	key[8] |= key[9] * 0x6a;

	if(type == 0)
	{
		key[7] &= 0x30004a24;
		key[9] ^= key[7] * 0x44;
		major_4 (key, key[11]);
	}

	if(type == 14)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		TRY(minor_7);
		major_18 (key, key[13]);
	}

	a += 0xefb98ee8;
	key[10] &= key[6] - 0x1286a10;

	if(type == 12)
	{
		key[9] += ROL(key[4], 9);
		TRY(minor_4);
		major_15 (key, key[17]);
	}

	if(type == 2)
	{
		TRY(minor_8);
		minor_20;
		major_3 (key, key[13]);
	}

	ROREQ(key[14], ROL(seed, 8));

	if(type == 9)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		minor_26;
		major_21 (key, key[5]);
	}

	a += 0xfa873e57;
	seed += 0x176cf052 - key[12];

	if(type == 15)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		key[13] *= 0xa02fe00;
		major_16 (key, seed);
	}

	if(type == 1)
	{
		key[13] *= ROR(key[3], 5);
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_1 (key, key[17]);
	}

	ROLEQ(key[8], key[4] | 0xf);

	if(type == 5)
	{
		key[9] ^= key[7] * 0x44;
		key[7] &= 0x1df23f52;
		major_14 (key, key[6]);
	}

	if(type == 4)
	{
		key[5] -= key[15];
		key[6] ^= 0x851242df;
		major_6 (key, seed);
	}

	a += 0xf183ce5d;
	key[13] *= key[2] * 0x65;

	if(type == 0)
	{
		key[14] |= key[3] ^ 0x4345732;
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		major_19 (key, key[10]);
	}

	if(type == 6)
	{
		minor_29;
		minor_22;
		major_25 (key, seed);
	}

	key[11] |= ROR(key[17], 29);

	if(type == 13)
	{
		key[10] *= key[10] - 0x5eae6bf;
		key[16] += 0x5e01d54b;
		major_17 (key, key[18]);
	}

	a += 0xebe46864;
	key[17] &= seed * 0x30;

	if(type == 7)
	{
		minor_27;
		TRY(minor_12 (0x65ec261));
		major_4 (key, key[0]);
	}

	if(type == 11)
	{
		key[14] |= key[3] ^ 0x4345732;
		key[0] += key[18] ^ 0x4ac16b8d;
		major_18 (key, key[16]);
	}

	key[13] |= key[3] * 0x3e;
}

void major_10 (u32 *key, u32 seed)
{
	int type = (key[4] ^ key[12] ^ key[17]) & 15;
	u32 a = 0x7a66df8;

	if(type == 9)
	{
		TRY(minor_12 (0x9febcd24));
		key[7] &= 0x259cf308;
		major_16 (key, key[14]);
	}

	if(type == 4)
	{
		key[6] ^= 0xa7e6f9b9;
		key[10] *= key[10] - 0x5eae6bf;
		major_1 (key, key[2]);
	}

	a += 0xfc318a14;
	key[9] += key[11] < a ? key[11] : key[9];

	if(type == 6)
	{
		TRY(minor_10 (0xece6bfa0));
		key[14] |= key[3] ^ 0x4345732;
		major_14 (key, seed);
	}

	if(type == 8)
	{
		TRY(minor_7);
		key[18] *= key[10] + 0x466e09cf;
		major_6 (key, key[12]);
	}

	if(type == 2)
	{
		key[12] += key[6] + 0x21d7bf61;
		key[14] |= key[3] ^ 0x4345732;
		major_9 (key, key[1]);
	}

	key[10] *= my_cos(seed) ? 0x16b578ee : key[2];

	if(type == 6)
	{
		minor_19;
		minor_29;
		major_2 (key, key[10]);
	}

	if(type == 13)
	{
		key[9] ^= key[7] * 0x44;
		minor_35;
		major_5 (key, key[10]);
	}

	a += 0xebded5c1;
	key[17] += seed * 0x4d;

	if(type == 1)
	{
		minor_26;
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_12 (key, seed);
	}

	if(type == 14)
	{
		TRY(minor_8);
		minor_31 (0x7);
		major_11 (key, key[19]);
	}

	if(type == 1)
	{
		key[19] ^= key[15] ^ 0x3574ed3;
		key[10] += 0x9f2550bd;
		major_13 (key, seed);
	}

	ROLEQ(seed, key[7] * 0xd);

	if(type == 12)
	{
		key[9] &= 0x2f23cdc6;
		key[10] *= key[10] - 0x5eae6bf;
		major_22 (key, key[2]);
	}

	if(type == 3)
	{
		minor_28;
		minor_26;
		major_8 (key, key[4]);
		key[12] *= key[12];
		TRY(minor_6);
		major_16 (key, key[18]);
	}

	a += 0xf428ad3b;
	key[4] += my_sin(key[0]) ? 0x1873296 : key[1];

	if(type == 10)
	{
		minor_26;
		TRY(minor_13);
		major_1 (key, key[8]);
	}

	if(type == 4)
	{
		key[12] *= 0xf44cb55;
		key[16] += 0x75a864cf;
		major_14 (key, key[4]);
	}

	key[12] += 0x1c0bd6db - key[11];

	if(type == 7)
	{
		TRY(minor_7);
		minor_26;
		major_6 (key, key[14]);
	}

	if(type == 2)
	{
		key[13] *= 0x17b441db;
		TRY(minor_12 (0x8951503f));
		major_9 (key, key[19]);
	}

	if(type == 11)
	{
		key[0] += key[6] * 0x3c;
		key[4] -= key[17] ^ 0x2217cf47;
		major_2 (key, key[8]);
	}

	a += 0x9b6e0949;
	key[18] -= key[6] * 0x2c;

	if(type == 7)
	{
		key[18] *= key[10] + 0x466e09cf;
		minor_29;
		major_5 (key, key[15]);
	}

	if(type == 0)
	{
		key[13] *= 0x1855aabc;
		TRY(minor_3);
		major_12 (key, seed);
	}

	if(type == 5)
	{
		key[18] *= key[10] + 0x466e09cf;
		key[9] ^= key[7] * 0x44;
		major_11 (key, key[10]);
	}

	key[6] ^= key[16] ^ 0x354e354d;

	if(type == 15)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		minor_28;
		major_13 (key, seed);
	}

	if(type == 8)
	{
		key[14] |= key[3] ^ 0x4345732;
		minor_32 (0x8a0e1ad7);
		major_22 (key, key[8]);
	}

	a += 0xbc5c43b8;
	seed += a ^ key[17];

	if(type == 0)
	{
		TRY(minor_8);
		minor_22;
		major_8 (key, key[8]);
	}

	if(type == 5)
	{
		key[6] += 0xfe07af0e - key[3];
		key[13] *= 0xa02fe00;
		major_16 (key, key[10]);
	}

	key[3] += key[13] + a;
}

void major_11 (u32 *key, u32 seed)
{
	int type = (key[6] ^ seed ^ key[14]) & 15;
	u32 a = 0x1e171745;

	if(type == 2)
	{
		key[19] ^= key[15] ^ 0x3574ed3;
		key[13] -= key[1];
		major_18 (key, key[0]);
	}

	if(type == 0)
	{
		key[7] ^= 0x414517ea;
		TRY(minor_4);
		major_15 (key, key[13]);
	}

	a += 0xfea91d85;
	key[14] &= seed * 0x3f;

	if(type == 10)
	{
		minor_19;
		key[9] &= 0x38063558;
		major_3 (key, key[10]);
	}

	if(type == 15)
	{
		minor_34;
		key[12] += 0x5c1481dd;
		major_21 (key, key[11]);
	}

	ROREQ(key[10], key[14] * 0x13);

	if(type == 8)
	{
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		key[12] ^= key[15] - 0xf5cfde0;
		major_16 (key, seed);
	}

	if(type == 4)
	{
		key[3] -= key[0] ^ 0x185f3b0d;
		key[6] ^= 0xa8115127;
		major_1 (key, key[5]);
	}

	a += 0xec342555;
	key[11] ^= seed - 0x3c17609c;

	if(type == 1)
	{
		key[12] += key[6] ^ 0x211f5e40;
		minor_29;
		major_14 (key, seed);
	}

	if(type == 7)
	{
		ROREQ(key[9], 11);
		TRY(minor_3);
		major_6 (key, key[11]);
	}

	key[14] += my_sin(key[9]) ? 0x2d3f1771 : key[11];

	if(type == 5)
	{
		TRY(minor_8);
		key[0] += key[18] ^ 0x4ac16b8d;
		major_9 (key, key[9]);
	}

	if(type == 2)
	{
		key[12] *= 0xf44cb55;
		minor_31 (0x1f);
		major_2 (key, key[13]);
	}

	if(type == 1)
	{
		key[9] &= 0x3f34e168;
		key[18] *= key[10] + 0x466e09cf;
		major_5 (key, seed);
	}

	a += 0xd7d6ea5d;
	key[18] &= key[17] + 0x21012257;

	if(type == 14)
	{
		key[6] += 0xfe07af0e - key[3];
		TRY(minor_2 (0x51f9a91a));
		major_12 (key, key[14]);
	}

	if(type == 12)
	{
		key[14] |= key[3] ^ 0x4345732;
		key[16] += 0x485c892b;
		major_18 (key, key[12]);
	}

	key[19] &= key[10] ^ 0x6fc516d5;

	if(type == 6)
	{
		TRY(minor_5);
		key[12] += key[6] ^ 0x211f5e40;
		major_15 (key, key[13]);
	}

	if(type == 11)
	{
		TRY(minor_6);
		minor_28;
		major_3 (key, seed);
	}

	a += 0xa7dc3618;
	key[8] ^= key[11] * 0x7b;

	if(type == 4)
	{
		key[2] *= key[3] + 0xd6863a6;
		minor_26;
		major_21 (key, key[4]);
	}

	if(type == 3)
	{
		key[19] ^= key[15] ^ 0x3574ed3;
		minor_17;
		major_16 (key, key[9]);
	}

	key[0] += key[13] + a;

	if(type == 0)
	{
		TRY(minor_2 (0xf10f9d87));
		key[12] ^= key[15] - 0xf5cfde0;
		major_1 (key, key[15]);
	}

	if(type == 9)
	{
		key[13] -= key[1];
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		major_14 (key, key[18]);
	}

	a += 0xc847e2da;
	seed *= key[8] - 0x44260e37;

	if(type == 3)
	{
		minor_26;
		key[10] += 0x8958821;
		major_6 (key, key[8]);
	}

	if(type == 13)
	{
		key[0] += key[18] ^ 0x4ac16b8d;
		key[7] ^= 0x129d6c5e;
		major_9 (key, seed);
	}

	key[2] &= ROL(key[19], a);
}

void major_12 (u32 *key, u32 seed)
{
	int type = (key[7] ^ seed ^ key[18]) & 15;
	u32 a = 0x9e24650;

	if(type == 15)
	{
		TRY(minor_7);
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		major_4 (key, key[17]);
	}

	a += 0xd768a22a;
	key[8] |= seed + 0xe43fc6b;

	if(type == 1)
	{
		minor_32 (0x979304f6);
		minor_26;
		major_18 (key, key[6]);
	}

	if(type == 8)
	{
		minor_20;
		TRY(minor_12 (0xf7131053));
		major_15 (key, key[13]);
	}

	key[19] ^= seed * 0x4b;

	if(type == 1)
	{
		key[13] *= 0x85695585;
		key[13] *= 0x2c9514d7;
		major_3 (key, key[17]);
	}

	if(type == 10)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		TRY(minor_1 (0x433a0094));
		major_21 (key, key[8]);
	}

	a += 0xa8ee2bb8;
	key[1] ^= key[14] * 0x16;

	if(type == 0)
	{
		key[13] *= 0x136644be;
		TRY(minor_6);
		major_16 (key, key[14]);
	}

	key[7] |= seed ^ 0xe857063;

	if(type == 4)
	{
		key[18] -=  key[13] ^ 0x154abcdf;
		key[12] += 0xf894616a;
		major_1 (key, key[6]);
	}

	if(type == 9)
	{
		TRY(minor_3);
		key[7] &= 0x3b887f26;
		major_14 (key, key[7]);
	}

	a += 0xf9dadd84;
	ROREQ(key[6], key[9] * a);

	if(type == 3)
	{
		TRY(minor_9 (0xcd88ea76));
		TRY(minor_8);
		major_6 (key, key[19]);
	}

	if(type == 13)
	{
		ROREQ(key[9], 5);
		TRY(ROREQ(key[1], 0x8));
		major_9 (key, key[1]);
	}

	key[6] -= key[17] < 0x417e2f7b ? key[17] : key[19];

	if(type == 5)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		minor_19;
		major_2 (key, key[1]);
	}

	if(type == 12)
	{
		key[13] -= key[1];
		key[0] += key[6] * 0x3c;
		major_5 (key, key[7]);
	}

	a += 0xbbbb0c69;
	key[6] |= my_sqrt(seed);

	if(type == 2)
	{
		key[2] ^= key[15] << 5;
		key[7] &= key[13] ^ 0x21aaf758;
		major_4 (key, seed);
	}

	key[2] ^= key[8] + 0x3e85747b;

	if(type == 0)
	{
		minor_20;
		key[9] &= 0x5a61aa8d;
		major_18 (key, key[14]);
	}

	if(type == 11)
	{
		key[6] += 0xfe07af0e - key[3];
		minor_32 (0xbf47f027);
		major_15 (key, key[3]);
	}

	a += 0xcbf2b7ba;
	key[2] &= seed;

	if(type == 3)
	{
		key[13] *= 0xc9cf079;
		key[18] -=  key[13] ^ 0x154abcdf;
		major_3 (key, key[4]);
	}

	if(type == 7)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		TRY(minor_4);
		major_21 (key, key[14]);
	}

	seed += key[9] - a;

	if(type == 14)
	{
		TRY(minor_8);
		key[9] ^= key[7] * 0x44;
		major_16 (key, seed);
	}

	a += 0xdcbe2049;
	key[18] += key[11] * 0x5b;

	if(type == 2)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		minor_17;
		major_1 (key, key[1]);
	}

	if(type == 6)
	{
		key[2] ^= key[15] << 5;
		key[18] -=  key[13] ^ 0x154abcdf;
		major_14 (key, seed);
	}

	key[4] ^= key[4] - a;
}

void major_13 (u32 *key, u32 seed)
{
	int type = (key[4] ^ seed ^ key[18]) & 15;
	u32 a = 0x26334b11;

	if(type == 12)
	{
		minor_29;
		key[9] |= key[7] ^ 0x2a19119f;
		major_15 (key, key[11]);
	}

	if(type == 4)
	{
		ROREQ(key[9], 10);
		key[12] ^= key[15] - 0xf5cfde0;
		major_3 (key, seed);
	}

	seed ^= key[1] * 0x6c;

	if(type == 1)
	{
		TRY(minor_12 (0xad86172c));
		key[12] ^= key[15] - 0xf5cfde0;
		major_21 (key, key[7]);
	}

	if(type == 9)
	{
		key[0] += key[6] * 0x3c;
		key[9] &= 0xa2cc0e51;
		major_16 (key, key[10]);
	}

	key[11] += key[8] - 0xef3b680;

	if(type == 5)
	{
		minor_20;
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		major_1 (key, seed);
	}

	if(type == 2)
	{
		key[12] += 0x1f3bc7f4;
		key[9] += ROL(key[4], 9);
		major_14 (key, key[3]);
	}

	a += 0xc4e4b2e0;
	key[19] -= seed ^ 0x42b04005;

	if(type == 8)
	{
		TRY(minor_13);
		key[0] += key[18] ^ 0x4ac16b8d;
		major_6 (key, key[2]);
	}

	if(type == 3)
	{
		TRY(minor_13);
		minor_17;
		major_9 (key, seed);
	}

	key[0] += my_sqrt(key[16]);

	if(type == 11)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		ROREQ(key[4], 0x19);
		major_2 (key, key[19]);
	}

	if(type == 13)
	{
		TRY(minor_4);
		minor_26;
		major_5 (key, key[7]);
	}

	a += 0xffbfae59;
	seed += key[17] | a;

	if(type == 1)
	{
		TRY(minor_1 (0xd3280a0));
		key[7] ^= 0x3eb9d37;
		major_12 (key, key[12]);
	}

	if(type == 14)
	{
		minor_28;
		minor_31 (0x1f);
		major_11 (key, seed);
	}

	ROREQ(key[2], key[15] < 0x3f2998c ? key[15] : seed);

	if(type == 5)
	{
		TRY(minor_12 (0x2dd0e73));
		minor_26;
		major_15 (key, seed);
	}

	if(type == 3)
	{
		TRY(minor_9 (0x21602b81));
		key[13] *= 0x52fa4a96;
		major_3 (key, seed);
	}

	a += 0xe6d8117d;
	key[4] += key[2] ^ 0x1579499;

	if(type == 10)
	{
		TRY(minor_7);
		key[5] += key[0] ^ 0x3e17add3;
		major_21 (key, key[5]);
	}

	if(type == 0)
	{
		TRY(minor_5);
		key[0] += key[6] * 0x3c;
		major_16 (key, key[8]);
	}

	seed -= key[2] * 0x74;

	if(type == 0)
	{
		key[13] -= key[1];
		key[19] ^= key[15] ^ 0x3574ed3;
		major_1 (key, seed);
	}

	if(type == 15)
	{
		key[9] &= 0x334ce7cf;
		TRY(minor_10 (0xbcbc7bb));
		major_14 (key, seed);
	}

	a += 0xdcef2ff8;
	key[10] -= key[10] | a;

	if(type == 6)
	{
		TRY(minor_3);
		TRY(minor_6);
		major_6 (key, key[11]);
	}

	if(type == 4)
	{
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		key[13] *= 0x9f7e2a0;
		major_9 (key, key[19]);
	}

	seed += key[17] ^ a;

	if(type == 7)
	{
		TRY(minor_8);
		key[18] *= key[10] + 0x466e09cf;
		major_2 (key, key[8]);
	}

	if(type == 2)
	{
		TRY(minor_3);
		key[9] |= key[7] ^ 0x2a19119f;
		major_5 (key, key[18]);
	}

	key[16] -= key[11] < 0x1e7d86ee ? key[11] : seed;
}

void major_14 (u32 *key, u32 seed)
{
	int type = (key[8] ^ seed ^ key[11]) % 14;
	u32 a = 0xf74450ff;

	if(type == 0)
	{
		TRY(minor_1 (0xe32bdca0));
		minor_30;
		major_23 (key, key[19]);
	}

	seed -= seed ^ a;

	if(type == 1)
	{
		minor_32 (0x788c78a4);
		key[13] -= key[1];
		major_24 (key, seed);
	}

	key[13] -= my_cos(key[3]) ? a : key[4];

	if(type == 9)
	{
		key[9] |= key[7] ^ 0x2a19119f;
		ROREQ(key[9], 11);
		major_19 (key, seed);
	}

	a += 0xd1cac405;
	key[9] ^= key[6] * 0x59;

	if(type == 7)
	{
		minor_26;
		key[6] += 0xfe07af0e - key[3];
		major_25 (key, key[11]);
	}

	if(type == 8)
	{
		key[13] -= key[1];
		key[4] ^= 0xb949718c;
		major_17 (key, key[7]);
	}

	key[1] ^= my_sin(seed) ? a : key[17];

	if(type == 13)
	{
		key[9] &= 0x59d432be;
		key[18] -=  key[13] ^ 0x154abcdf;
		major_4 (key, key[4]);
	}

	a += 0xd200d425;
	key[17] += key[13] < 0xac24eb8 ? key[13] : key[9];

	if(type == 5)
	{
		key[4] ^= 0x3bcc51a7;
		key[12] += 0x4ec6cf36;
		major_18 (key, key[1]);
	}

	seed |= ROR(key[18], 11);

	if(type == 3)
	{
		minor_20;
		key[5] -= key[15];
		major_15 (key, key[0]);
	}

	a += 0xeb81800d;
	key[4] += seed + 0xf65efbd;

	if(type == 10)
	{
		TRY(minor_5);
		key[2] *= key[3] + 0xd6863a6;
		major_3 (key, key[5]);
	}

	if(type == 11)
	{
		key[7] &= 0xdf76eba8;
		TRY(minor_6);
		major_21 (key, seed);
	}

	key[4] ^= ROL(key[8], a);

	if(type == 6)
	{
		TRY(minor_10 (0xec30bd82));
		key[2] *= key[3] + 0xd6863a6;
		major_16 (key, key[13]);
	}

	a += 0xdf0fa672;
	seed *= key[6] + 0x6bbeb974;

	if(type == 2)
	{
		key[18] *= key[10] + 0x466e09cf;
		key[2] *= key[3] + 0xd6863a6;
		major_1 (key, key[6]);
	}

	key[16] -= key[2] * a;

	if(type == 12)
	{
		key[19] ^= key[15] ^ 0x3574ed3;
		ROREQ(key[9], 15);
		major_23 (key, key[14]);
	}

	ROREQ(key[13], my_sqrt(seed));

	if(type == 4)
	{
		minor_19;
		minor_17;
		major_24 (key, key[0]);
	}

	if(type == 0)
	{
		key[7] ^= 0xc9d1f4a2;
		minor_26;
		major_19 (key, seed);
	}

	key[12] -= my_sin(key[10]) ? 0x2818ae3c : seed;
}

void major_15 (u32 *key, u32 seed)
{
	int type = (key[17] ^ seed ^ key[19]) % 9;
	u32 a = 0xf9976b51;

	ROREQ(key[19], key[19] + 0xa);

	if(type == 4)
	{
		minor_19;
		key[6] ^= 0xf4c1a1c8;
		minor_37 (key);
	}

	key[5] ^= seed + 0x1ff8749d;

	if(type == 5)
	{
		ROREQ(key[4], 0x19);
		key[9] += ROL(key[4], 9);
		minor_36 (key);
	}

	a += 0xb231d4bd;
	key[13] ^= key[15] + 0x19ad9d3;

	if(type == 0)
	{
		key[14] |= key[3] ^ 0x4345732;
		minor_26;
		major_23 (key, key[13]);
	}

	ROREQ(key[3], my_sqrt(key[9]));

	if(type == 1)
	{
		key[16] += 0x188ae78f;
		key[2] ^= key[15] << 5;
		major_24 (key, key[12]);
	}

	a += 0xd6800e99;
	seed ^= key[12] ^ a;

	if(type == 0)
	{
		key[14] |= key[3] ^ 0x4345732;
		key[7] &= 0x97ea531;
		major_19 (key, key[6]);
	}

	ROLEQ(key[0], a & seed);

	if(type == 7)
	{
		minor_20;
		TRY(minor_9 (0xd3d79cb4));
		major_25 (key, key[6]);
	}

	a += 0xbc7cde7c;
	key[18] ^= key[9] - 0x5606038;

	if(type == 3)
	{
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		key[16] += 0x6a07a3d0;
		major_17 (key, key[8]);
	}

	key[9] |= my_sin(key[7]) ? a : key[6];

	if(type == 2)
	{
		key[18] *= key[10] + 0x466e09cf;
		TRY(minor_6);
		major_4 (key, key[1]);
	}

	if(type == 6)
	{
		minor_27;
		minor_22;
		major_18 (key, key[0]);
	}
}

void major_16 (u32 *key, u32 seed)
{
	int type = (key[11] ^ seed ^ key[5]) % 12;
	u32 a = 0x16bfb62c;

	if(type == 5)
	{
		key[2] *= key[3] + 0xd6863a6;
		minor_22;
		minor_37 (key);
	}

	a += 0xf43d9880;
	key[4] ^= seed - a;

	if(type == 2)
	{
		TRY(minor_5);
		key[0] += key[18] ^ 0x4ac16b8d;
		minor_36 (key);
	}

	key[15] -= a ^ seed;

	if(type == 0)
	{
		TRY(minor_2 (0x80e3e69e));
		ROREQ(key[9], 12);
		major_23 (key, key[4]);
	}

	a += 0xc1c9f0b0;
	key[8] ^= my_sqrt(key[16]);

	if(type == 3)
	{
		key[9] ^= 0x8e61a4f;
		key[13] -= key[1];
		major_24 (key, seed);
	}

	if(type == 10)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[13] *= 0xa02fe00;
		major_19 (key, key[6]);
	}

	seed -= seed & 0x179da692;

	if(type == 4)
	{
		TRY(minor_8);
		TRY(minor_5);
		major_25 (key, key[0]);
	}

	a += 0xff6e78c7;
	key[8] ^= key[15] * 0x5f;

	if(type == 0)
	{
		minor_32 (0x6191efec);
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		major_17 (key, key[9]);
	}

	key[6] &= my_sin(seed) ? a : key[14];

	if(type == 9)
	{
		key[6] += 0xfe07af0e - key[3];
		key[0] += key[18] ^ 0x4ac16b8d;
		major_4 (key, key[0]);
	}

	if(type == 6)
	{
		key[7] |= 0xa885099;
		key[9] ^= 0xdd34e6b;
		major_18 (key, seed);
	}

	a += 0xfcc12c95;
	seed -= my_cos(key[19]) ? 0xc818c81 : key[19];

	if(type == 7)
	{
		key[12] += 0x5e6f4861;
		key[18] -=  key[13] ^ 0x154abcdf;
		major_15 (key, key[14]);
	}

	key[10] += key[1] + 0x217f7a00;

	if(type == 1)
	{
		key[0] += key[18] ^ 0x4ac16b8d;
		minor_27;
		major_3 (key, key[17]);
	}

	a += 0xc4c948f6;
	key[5] &= ROR(key[0], a);

	if(type == 8)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[9] ^= key[7] * 0x44;
		major_21 (key, key[13]);
	}

	if(type == 11)
	{
		minor_30;
		key[13] += key[15] < 0x137bffeb ? key[15] : key[11];
		minor_37 (key);
	}

	key[12] |= ROL(key[7], a);
}

void major_17 (u32 *key, u32 seed)
{
	int type = (key[8] ^ key[7] ^ key[12]) % 6;
	u32 a = 0xb87e62ab;

	key[1] |= key[4] ^ 0x10104d4;

	if(type == 3)
	{
		minor_20;
		ROREQ(key[9], 12);
		minor_37 (key);
	}

	a += 0xc39ce241;
	seed = ((seed ^ 0x1ea9da8) + seed) * key[18] * 0xd;

	if(type == 0)
	{
		TRY(minor_1 (0x10381ff0));
		key[2] *= key[3] + 0xd6863a6;
		minor_36 (key);
	}

	key[14] += key[12] * 0x19;
	a += 0xeb82854b;
	key[2] -= my_sqrt(key[5]);

	if(type == 4)
	{
		key[16] += 0x81063b22;
		key[9] ^= key[7] * 0x44;
		major_23 (key, seed);
	}

	key[6] &= key[4] - a;
	a += 0xd6381793;
	key[1] ^= key[16] + 0x988db31;

	if(type == 0)
	{
		key[7] ^= 0xa98896dd;
		TRY(minor_3);
		major_24 (key, key[6]);
	}

	key[6] += ROR(seed, a);
	a += 0xec144aec;
	seed -= key[0] < a ? key[0] : key[3];

	if(type == 2)
	{
		minor_35;
		key[12] ^= key[15] - 0xf5cfde0;
		major_19 (key, seed);
	}

	seed *= my_sqrt(seed);
	a += 0xf730c712;
	key[5] *= my_cos(seed) ? a : key[19];

	if(type == 5)
	{
		minor_28;
		key[13] *= 0xa02fe00;
		major_25 (key, key[13]);
	}
}

void major_18 (u32 *key, u32 seed)
{
	int type = (key[14] ^ key[11] ^ key[17]) & 7;
	u32 a = 0x128c2b75;

	key[11] ^= ROR(key[13], a);

	if(type == 5)
	{
		key[6] += key[19] - 0x3f5675d6;
		key[9] ^= 0x94d017f;
		minor_37 (key);
	}

	ROREQ(key[3], key[16] * 0xf);

	if(type == 2)
	{
		key[5] += key[0] ^ 0x3e17add3;
		minor_34;
		minor_36 (key);
	}

	a += 0xc7be5cf7;
	key[11] -= my_sqrt(key[9]);
	key[12] += 0x17267c5b - key[11];

	if(type == 3)
	{
		minor_31 (0xb);
		key[7] &= key[13] ^ 0x21aaf758;
		major_23 (key, key[0]);
	}

	a += 0xc741f5fc;
	key[17] ^= seed ^ 0x35eddea4;

	if(type == 0)
	{
		key[10] += 0x3409139c;
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		major_24 (key, key[6]);
	}

	key[6] *= key[17] + 0xb89b51c;

	if(type == 1)
	{
		key[6] += 0xfe07af0e - key[3];
		TRY(minor_2 (0x90254266));
		major_19 (key, key[6]);
	}

	a += 0xb5c971a6;
	key[19] ^= key[3] < a ? key[3] : key[1];
	key[15] ^= key[12] * 0x17;

	if(type == 7)
	{
		key[13] *= ROR(key[3], 5);
		key[13] *= ROR(key[3], 5);
		major_25 (key, key[9]);
	}

	a += 0xb8879326;
	key[10] += 0x395f1d29 - seed;

	if(type == 0)
	{
		key[12] += 0x2272516f;
		key[13] *= 0x48e3e7ac;
		major_17 (key, key[16]);
	}

	ROLEQ(key[1], ROL(key[8], a));
	a += 0xb9e33a61;
	seed -= key[9] ^ a;

	if(type == 6)
	{
		TRY(minor_2 (0x10b4eaef));
		key[12] += 0x222fe8f5;
		major_4 (key, seed);
	}

	ROLEQ(key[18], key[7] & 0x11);
}

void major_19 (u32 *key, u32 seed)
{
	int type = (key[18] ^ key[6] ^ key[15]) & 3;
	u32 a = 0xe42c799d;

	seed *= key[15] * 0x3c02927;
	ROREQ(seed, seed * 0x7);

	if(type == 0)
	{
		key[12] += key[6] ^ 0x211f5e40;
		key[9] ^= 0x6b4bfbe3;
		minor_37 (key);
	}

	a += 0xd4fa2213;
	seed ^= key[6] ^ 0xc1fcda0;
	key[5] -= my_cos(key[6]) ? a : key[10];

	if(type == 0)
	{
		key[9] ^= 0x703e6c86;
		key[16] += 0xbb78136d;
		minor_36 (key);
	}

	a += 0xb1a3e457;
	seed *= key[19] + 0x11500e47;
	key[3] ^= ROL(key[4], 20);

	if(type == 3)
	{
		key[2] ^= key[15] << 5;
		key[19] ^= key[15] ^ 0x3574ed3;
		major_23 (key, key[15]);
	}

	a += 0xc6b6b08a;
	key[13] -= my_sqrt(seed);
	ROREQ(seed, my_cos(seed) ? 7 : key[10]);
	key[16] = key[15] * key[16] * 0x4a;

	if(type == 1)
	{
		key[7] ^= 0xb3bb63f;
		key[4] -= key[17] ^ 0x2217cf47;
		major_24 (key, seed);
	}
}

void major_21 (u32 *key, u32 seed)
{
	int type = (key[1] ^ key[0] ^ key[16]) % 11;
	u32 a = 0xcb1d507c;

	if(type == 2)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[12] ^= key[15] - 0xf5cfde0;
		minor_37 (key);
	}

	key[5] -= seed;

	if(type == 8)
	{
		key[16] += 0x2b058ae8;
		key[6] += 0xfe07af0e - key[3];
		minor_36 (key);
	}

	key[17] ^= ROL(key[18], a);

	if(type == 4)
	{
		key[2] *= key[3] + 0xd6863a6;
		minor_32 (0x79fb5201);
		major_23 (key, key[7]);
	}

	a += 0xbeb660bf;
	key[0] ^= my_sqrt(key[12]);

	if(type == 0)
	{
		key[19] ^= key[15] ^ 0x3574ed3;
		TRY(minor_5);
		major_24 (key, key[2]);
	}

	key[10] ^= seed * 0x6c;

	if(type == 9)
	{
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		minor_32 (0x6ddf8c10);
		major_19 (key, key[10]);
	}

	a += 0xfd85df53;
	key[8] -= my_cos(key[12]) ? a : seed;

	if(type == 7)
	{
		minor_19;
		minor_29;
		major_25 (key, key[1]);
	}

	seed ^= my_sin(seed) ? 0x2c99fade : key[14];

	if(type == 1)
	{
		TRY(minor_12 (0x3fcf3163));
		key[9] ^= key[7] * 0x44;
		major_17 (key, seed);
	}

	a += 0xf9805d1e;
	key[15] += my_cos(key[11]) ? 0x1bec01f : seed;

	if(type == 5)
	{
		key[13] *= 0x1bd5157f;
		key[6] += key[19] - 0x3f5675d6;
		major_4 (key, key[15]);
	}

	ROREQ(key[1], a * key[16]);

	if(type == 0)
	{
		TRY(minor_10 (0xfde30e03));
		key[9] |= key[7] ^ 0x2a19119f;
		major_18 (key, seed);
	}

	a += 0xda780d6d;
	key[7] &= key[15] * 0xa8f285;

	if(type == 10)
	{
		key[7] ^= 0xef011757;
		ROREQ(key[9], 9);
		major_15 (key, key[13]);
	}

	if(type == 3)
	{
		key[12] += key[6] + 0x21d7bf61;
		key[6] += key[19] - 0x3f5675d6;
		major_3 (key, key[10]);
	}

	key[3] *= my_sin(key[8]) ? a : key[2];

	if(type == 6)
	{
		key[9] += ROL(key[4], 9);
		minor_22;
		minor_37 (key);
	}

	key[11] ^= key[17] * 0x44;
}

void major_22 (u32 *key, u32 seed)
{
	int type = (key[5] ^ key[0] ^ seed) & 15;
	u32 a = 0x7c36f793;

	if(type == 3)
	{
		TRY(minor_6);
		minor_31 (0x13);
		major_3 (key, seed);
	}

	if(type == 0)
	{
		key[6] ^= 0x6066818c;
		key[13] -= key[1];
		major_21 (key, key[2]);
	}

	a += 0xfb3de877;
	key[14] ^= ROL(key[16], 22);

	if(type == 12)
	{
		key[10] += 0x830ba927;
		minor_32 (0x6f3a3876);
		major_16 (key, key[8]);
	}

	if(type == 1)
	{
		minor_34;
		key[16] += 0x1bc7b861;
		major_1 (key, key[6]);
	}

	key[12] ^= key[11] < 0x521b2180 ? key[11] : key[9];

	if(type == 1)
	{
		minor_31 (0x12);
		key[0] += key[6] * 0x3c;
		major_14 (key, key[15]);
	}

	if(type == 8)
	{
		TRY(minor_5);
		key[18] *= key[10] + 0x466e09cf;
		major_6 (key, key[13]);
	}

	if(type == 4)
	{
		key[9] += ROL(key[4], 9);
		key[2] *= key[3] + 0xd6863a6;
		major_9 (key, key[16]);
	}

	a += 0xff46fc82;
	key[18] &= my_sqrt(key[9]);

	if(type == 5)
	{
		TRY(minor_4);
		key[2] ^= key[15] << 5;
		major_2 (key, key[2]);
	}

	if(type == 6)
	{
		key[2] *= key[3] + 0xd6863a6;
		key[6] += 0xfe07af0e - key[3];
		major_5 (key, key[14]);
	}

	key[18] -= key[16] * 0x77;

	if(type == 9)
	{
		key[4] ^= 0xa09619f7;
		ROREQ(key[4], 0x19);
		major_12 (key, key[10]);
	}

	if(type == 10)
	{
		key[12] ^= key[15] - 0xf5cfde0;
		key[13] *= 0x6cd0251e;
		major_11 (key, key[0]);
	}

	if(type == 6)
	{
		key[2] *= key[3] + 0xd6863a6;
		ROREQ(key[4], 0x1a);
		major_13 (key, seed);
	}

	a += 0xd2750177;
	key[13] ^= a ^ seed;

	if(type == 2)
	{
		key[10] += 0x6467451;
		key[4] -= key[17] ^ 0x2217cf47;
		major_3 (key, key[2]);
	}

	if(type == 7)
	{
		key[4] -= key[17] ^ 0x2217cf47;
		key[0] += key[18] ^ 0x4ac16b8d;
		major_21 (key, key[0]);
	}

	key[6] -= my_sqrt(key[10]);

	if(type == 3)
	{
		TRY(minor_9 (0x5b9d1f9));
		key[10] += 0x8958821;
		major_16 (key, key[8]);
	}

	if(type == 4)
	{
		key[13] -= key[1];
		key[13] *= 0x72494c9c;
		major_1 (key, seed);
	}

	if(type == 13)
	{
		key[12] += key[6] + 0x21d7bf61;
		key[13] *= ROR(key[3], 5);
		major_14 (key, key[2]);
	}

	a += 0xf215c83c;
	seed -= ROR(key[8], 17);

	if(type == 15)
	{
		key[19] += 0x12b9e29d - key[12];
		key[0] += key[6] * 0x3c;
		major_6 (key, key[4]);
	}

	if(type == 2)
	{
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		minor_20;
		major_9 (key, key[16]);
	}

	ROREQ(key[14], seed - a);

	if(type == 11)
	{
		key[19] ^= my_cos(key[9]) ? 0x57337b8 : key[14];
		key[9] += ROL(key[4], 9);
		major_2 (key, key[9]);
	}

	if(type == 5)
	{
		key[19] += 0x12b9e29d - key[12];
		TRY(minor_4);
		major_5 (key, key[6]);
	}

	if(type == 0)
	{
		minor_20;
		key[6] ^= 0xa9c74969;
		major_12 (key, key[14]);
	}

	a += 0xce762e07;
	key[8] ^= ROR(seed, a);

	if(type == 14)
	{
		key[12] += 0x49fc5980;
		key[3] -= key[0] ^ 0x185f3b0d;
		major_11 (key, seed);
	}

	key[0] += my_sin(key[0]) ? a : key[14];
}

void major_23 (u32 *key, u32 seed)
{
	int type = seed & 1;

	key[4] += key[8] - 0x16f911e4;
	key[9] ^= key[2] * 11;
	key[10] ^= key[7] < 0x402226f ? key[7] : key[2];
	seed |= key[17] - 0x1e97aeb;
	seed |= key[14] < 0xf3b1e0b3 ? key[14] : key[5];

	if(type == 0)
	{
		key[7] &= key[13] ^ 0x21aaf758;
		minor_32 (0x640f077d);
		minor_37 (key);
	}

	key[1] -= key[19] * 0x64;
	key[1] += seed - 0x18d1b90;
	key[7] -= key[3] ^ 0x44de1958;
	key[11] ^= ROL(key[2], 9);
	key[17] += ROL(key[12], 27);

	if(type == 0)
	{
		TRY(minor_9 (0xdc306f47));
		key[9] ^= key[7] * 0x44;
		minor_36 (key);
	}

	ROREQ(key[7], key[13]);
}

void major_24 (u32 *key, u32 seed)
{
	int type = (key[2] ^ seed ^ key[7]) % 3;

	seed *= my_cos(seed) ? 0x6be8f94 : seed;
	key[2] ^= key[2] + 0x3786364b;
	ROLEQ(key[17], seed - 0x10);

	if(type == 0)
	{
		minor_35;
		minor_27;
		minor_37 (key);
	}

	seed += key[3] ^ 0xff342d3c;
	ROLEQ(seed, my_sin(key[11]) ? 17 : key[0]);
	key[5] += my_sin(key[16]) ? 0x3af2a8e2 : key[16];

	if(type == 0)
	{
		TRY(minor_5);
		key[2] *= key[3] + 0xd6863a6;
		minor_36 (key);
	}

	key[13] ^= my_cos(key[16]) ? 0xf6951daa : key[1];
	key[18] |= key[17] & 0x6361a322;
	seed += my_sqrt(key[10]);

	if(type == 1)
	{
		key[13] *= ROR(key[3], 5);
		key[13] *= 0xb25cb20f;
		major_23 (key, key[15]);
	}
}

void major_25 (u32 *key, u32 seed)
{
	int type = (key[7] ^ key[2] ^ seed) % 5;

	key[2] -= 0x31b8a51 & seed;

	if(type == 3)
	{
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		key[9] &= 0x49a7e0c7;
		minor_37 (key);
	}

	key[1] &= ROR(seed, 29);
	ROLEQ(key[12], my_cos(key[1]) ? 27: key[5]);

	if(type == 2)
	{
		TRY(minor_4);
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		minor_36 (key);
	}

	ROREQ(seed, my_sqrt(seed));
	key[17] += key[19] * 0x7a;

	if(type == 0)
	{
		key[10] += 0x8958821;
		key[18] *= key[10] + 0x466e09cf;
		major_23 (key, key[10]);
	}

	ROREQ(key[18], my_cos(key[6]) ? 0x11 : key[1]);
	seed ^= 0xc63d7671 * seed;

	if(type == 4)
	{
		TRY(minor_7);
		key[9] ^= 0x3480eee;
		major_24 (key, seed);
	}

	key[10] -= my_sqrt(seed);
	key[11] &= seed * 0x3f;

	if(type == 0)
	{
		key[18] *= key[10] + 0x466e09cf;
		key[13] *= 0x6ff7af6a;
		major_19 (key, key[17]);
	}

	ROLEQ(key[1], key[15] + 0x19);
}

void mix (u32 *key, u32 seed)
{
	int type = (key[5] ^ key[9] ^ key[19]) & 15;

	switch (type) {
	case 0:
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		TRY(minor_5);
		major_5 (key, 0x45835eb3);
		break;
	case 5:
		TRY(minor_8);
		key[12] += 0x33bd47dd;
		major_6 (key, key[9]);
		break;
	case 8:
		minor_28;
		key[6] += key[19] - 0x3f5675d6;
		major_9 (key, key[10]);
		break;
	case 9:
		TRY(minor_7);
		TRY(minor_13);
		major_14 (key, seed);
		break;
	case 10:
		TRY(minor_5);
		key[6] ^= 0x33b5c9ac;
		major_2 (key, key[0]);
		break;
	}

	key[2] ^= key[6] + 0x1847de17;

	switch (type) {
	case 1:
		key[9] ^= 0x1df05ea2;
		key[19] += 0x12b9e29d - key[12];
		major_12 (key, 0x45835eb3);
		break;
	case 10:
		minor_28;
		key[12] *= key[12];
		major_11 (key, key[6]);
		break;
	}

	key[19] += key[12] * 0x68;

	switch (type) {
	case 2:
		minor_28;
		key[12] ^= key[15] - 0xf5cfde0;
		major_13 (key, key[19]);
		key[6] += key[19] - 0x3f5675d6;
		key[12] += 0x602283af;
		major_22 (key, key[0]);
		break;
	case 6:
		TRY(minor_1 (0x706a6bc));
		TRY(minor_2 (0x82b598a1));
		major_8 (key, 0x45835eb3);
		break;
	}

	key[7] -= key[14] & 0x1ada7fa;

	switch (type) {
	case 4:
		minor_20;
		key[12] += key[6] + 0x21d7bf61;
		major_10 (key, key[16]);
		break;
	case 6:
		TRY(minor_7);
		TRY(minor_2 (0xd2950f8c));
		major_7 (key, 0x45835eb3);
		break;
	}

	seed = (key[0] + 0xd092d1bb) & 0x45835eb3;

	switch (type) {
	case 3:
		TRY(minor_5);
		key[7] &= key[13] ^ 0x21aaf758;
		major_9 (key, key[17]);
		break;
	case 4:
		key[13] -= key[1];
		key[18] *= key[10] + 0x466e09cf;
		major_14 (key, key[5]);
		break;
	case 7:
		TRY(minor_9 (0x6d32760));
		key[9] ^= key[7] * 0x44;
		major_6 (key, key[8]);
		break;
	}

	ROLEQ(key[8], key[3] ^ 0x6);

	switch (type) {
	case 0:
		key[7] &= key[13] ^ 0x21aaf758;
		key[14] |= key[3] ^ 0x4345732;
		major_12 (key, key[9]);
		break;
	case 5:
		minor_22;
		minor_29;
		major_2 (key, key[1]);
		break;
	case 9:
		key[13] -= key[1];
		TRY(minor_7);
		major_5 (key, key[2]);
		break;
	}

	key[9] *= key[14] | 0xbbf1fbef;

	switch (type) {
	case 15:
		key[12] += key[6] ^ 0x211f5e40;
		key[5] += key[0] ^ 0x3e17add3;
		major_11 (key, key[8]);
		break;
	case 12:
		key[19] ^= key[15] ^ 0x3574ed3;
		minor_30;
		major_13 (key, key[16]);
		break;
	}

	seed *= my_sqrt(key[1]);

	switch (type) {
	case 11:
		minor_32 (0x678aae2c);
		minor_22;
		major_22 (key, key[11]);
		break;
	case 14:
		key[13] *= ROR(key[3], 5);
		minor_35;
		major_8 (key, key[18]);
		break;
	case 7:
		ROREQ(key[4], 0x2);
		ROREQ(key[9], 4);
		major_10 (key, key[0]);
		break;
	}

	key[3] -= key[7] ^ 0x4e46f05d;

	switch (type) {
	case 3:
		key[7] ^= 0xeda01e71;
		key[13] -= key[1];
		major_7 (key, key[11]);
		break;
	case 8:
		key[2] ^= my_sin(key[13]) ? 0xfd08092 : key[10];
		key[9] += ROL(key[4], 9);
		major_14 (key, key[2]);
		break;
	}

	key[19] ^= 0xb1bdd560 ^ seed;

	switch (type) {
	case 1:
		key[12] ^= key[15] - 0xf5cfde0;
		key[12] *= key[12];
		major_9 (key, key[5]);
		break;
	case 13:
		key[18] *= key[10] + 0x466e09cf;
		key[2] ^= key[15] << 5;
		major_6 (key, key[15]);
		break;
	}

	key[6] ^= my_sqrt(key[5]);
}
