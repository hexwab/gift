/*
 * $Id: enc_type_2.c,v 1.10 2003/07/13 11:15:09 weinholt Exp $
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
 * This file was relayed to me and is originally from Raimar Falke.
 * I cleaned it up a bit to save bandwidth.
 * Used for encryption version 0x02
 * Then it was cleaned up a whole lot more...
 * And then Thingol cleaned it up even more, many thanks!
 */

typedef unsigned char	u8;
typedef unsigned int	u32;

static void mix_major0 (u32 *state, u32 extra_state);

/* this all works on unsigned ints so endianess is not an issue */

void enc_type_2 (unsigned int *key, unsigned int seed)
{
	mix_major0 (key, seed);
}

static void mix_major0 (u32 *state, u32 extra_state);
static void mix_major1 (u32 *state, u32 extra_state);
static void mix_major2 (u32 *state, u32 extra_state);
static void mix_major3 (u32 *state, u32 extra_state);
static void mix_major4 (u32 *state, u32 extra_state);
static void mix_major5 (u32 *state, u32 extra_state);
static void mix_major6 (u32 *state, u32 extra_state);
static void mix_major7 (u32 *state, u32 extra_state);
static void mix_major8 (u32 *state, u32 extra_state);
static void mix_major9 (u32 *state, u32 extra_state);
static void mix_major10 (u32 *state, u32 extra_state);
static void mix_major11 (u32 *state, u32 extra_state);
static void mix_major12 (u32 *state, u32 extra_state);
static void mix_major13 (u32 *state, u32 extra_state);
static void mix_major14 (u32 *state, u32 extra_state);
static void mix_major15 (u32 *state, u32 extra_state);
static void mix_major16 (u32 *state, u32 extra_state);
static void mix_major17 (u32 *state, u32 extra_state);
static void mix_major18 (u32 *state, u32 extra_state);
static void mix_major19 (u32 *state, u32 extra_state);
static void mix_major20 (u32 *state, u32 extra_state);
static void mix_major21 (u32 *state, u32 extra_state);
static void mix_major22 (u32 *state, u32 extra_state);
static void mix_major23 (u32 *state, u32 extra_state);
static void mix_major24 (u32 *state, u32 extra_state);

#define mix_minor20 ROREQ (state[12], state[14] * 0x3)
#define mix_minor21 state[12] *= state[1] * 0x4b4f2e1
#define mix_minor22 state[2] *= state[10] + 0xfa1f1e0b
#define mix_minor23 state[19] += state[19] ^ 0x43b6b05
#define mix_minor24 state[18] -= ROR (state[4], 18)
#define mix_minor25 state[0] &= state[10] + 0xfc9be92d
#define mix_minor26 state[9] ^= state[3] + 0xbe5fec7d
#define mix_minor27 state[6] *= state[15] | 0x46afede0
#define mix_minor28 state[17] -= state[6] * 0x1b677cc8
#define mix_minor29 state[14] &= state[15] + 0xfc471d2b
#define mix_minor30 state[19] += state[16] + 0x24a7d94d
#define mix_minor31 state[15] *= state[0] ^ 0x48ad05f2
#define mix_minor32 state[16] -= state[4] - 0xbb834311
#define mix_minor33 state[8] += ROL (state[4], 26)
#define mix_minor34 state[13] *= state[18] + 0xac048a2
#define mix_minor35 state[16] &= state[18] + 0xe832eb88
#define mix_minor36 state[4] -= state[1] - 0xe6f17893
#define mix_minor37 state[6] *= state[7] | 0x17b60bb5
#define mix_minor38 state[15] += ROL (state[12], 16)
#define mix_minor39 state[6] &= state[10] + 0xfd7af7e
#define mix_minor40 ROREQ (state[7], state[18] & 2)
#define mix_minor41 ROREQ (state[17], state[7] ^ 3)
#define mix_minor42 state[0] ^= state[8] + 0xeee530d5
#define mix_minor43 state[10] += state[1] + 0xc484cfa2
#define mix_minor44 state[16] += state[5] ^ 0x19a836dc
#define mix_minor45 state[17] += state[7] + 0xd68a11c3
#define mix_minor46 state[17] += ROL (state[7], 19)
#define mix_minor47 state[18] -= state[6] * 0x368eaf4e
#define mix_minor48 ROREQ (state[2], state[7] ^ 3)
#define mix_minor49 state[19] |= state[5] + 0xda7c6c8e
#define mix_minor50 state[6] *= ROR (state[2], 12)
#define mix_minor51 state[14] += state[18] + 0xf655a040
#define mix_minor52 state[11] += state[19] * 0x251df1bd
#define mix_minor53 state[11] -= state[0] ^ 0x51a859c
#define mix_minor54 state[18] += state[6] + 0xdcccfc5
#define mix_minor55 state[16] -= state[18] ^ 0x39848960
#define mix_minor56 state[14] ^= state[19] + 0x1a6f3b29
#define mix_minor57 state[12] &= state[5] + 0x4ef1335a
#define mix_minor58 state[14] *= state[13] + 0xdb61abf8
#define mix_minor59 state[18] ^= state[19] * 0x378f67
#define mix_minor60 state[18] ^= state[4] * 0x2dd2a2fe
#define mix_minor61 state[16] -= state[4] - 0xe357b476
#define mix_minor62 state[6] *= state[16] * 0x381203
#define mix_minor63 state[10] |= ROR (state[11], 24)
#define mix_minor64 state[10] ^= state[5] + 0x147c80d5
#define mix_minor65 state[16] ^= state[3] * 0x27139980
#define mix_minor66 ROREQ (state[15], state[17])
#define mix_minor67 state[14] &= ROL (state[19], 6)
#define mix_minor68 state[8] *= state[0] * 0x1a4c02dd
#define mix_minor69 state[16] ^= state[14] + 0xfddb63a2

#define ROR(value, count) ((value) >> ((count) & 0x1f) | ((value) << (32 - (((count) & 0x1f)))))
#define ROL(value, count) ((value) << ((count) & 0x1f) | ((value) >> (32 - (((count) & 0x1f)))))

#define ROREQ(value, count) value = ROR(value, count)
#define ROLEQ(value, count) value = ROL(value, count)

void mix_major0 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[15] ^ state[19] ^ extra_state) % 11;
	state[6] *= state[8] * 0x1bb4a70d;
	state[12] += state[14] + 0xe087bd96;
	extra_state &= state[13] | 0x39367989;

	if (a == 7) {
		mix_minor30;
		mix_minor41;
		mix_minor45;
		mix_major3 (state, state[19]);
	}

	state[14] -= ROR (extra_state, 31);
	state[8] ^= extra_state & 0x8e30c76;
	state[3] *= state[12] ^ 0xd05f635;
	state[10] += state[10] + 0xa92dc43a;

	if (a == 0) {
		state[0] += 0xde3b3b9a;
		mix_minor46;
		state[3] += 0x8600800;
		mix_major14 (state, state[5]);
	}

	extra_state += state[17] + 0xff92b824;
	state[1] += state[3] ^ 0x62c448c0;
	state[8] ^= state[3] ^ 0x43c25efd;
	state[9] ^= ROL (state[9], 16);

	if (a == 5) {
		mix_minor53;
		state[3] += 0x8502040;
		mix_minor54;
		mix_major9 (state, state[5]);
	}

	state[3] -= state[2] - 0xef553b21;
	state[18] += state[13] + 0x3b26991e;

	if (a == 4) {
		mix_minor39;
		mix_minor42;
		mix_minor35;
		mix_major10 (state, state[7]);
	}

	state[12] += state[11] & 0x4be050d;
	state[17] ^= ROR (extra_state, 8);
	ROREQ (state[8], state[16] + 0x17);
	state[12] *= state[8] + 0xf3910fa;

	if (a == 2) {
		mix_minor58;
		mix_minor59;
		mix_minor38;
		mix_major2 (state, extra_state);
	}

	state[8] += extra_state + 0x4088eb5f;
	state[5] &= state[7] ^ 0x1387a250;
	state[2] |= state[1] ^ 0x47f3a78b;
	state[17] |= state[10] * 0x1d208465;

	if (a == 1) {
		mix_minor39;
		mix_minor49;
		mix_minor50;
		mix_major7 (state, state[9]);
	}

	state[1] -= extra_state & 0x4be5deac;
	state[4] += state[15] & 0x3496b61a;

	if (a == 10) {
		mix_minor55;
		mix_minor61;
		state[8] += 0x82e5ca1;
		mix_major21 (state, state[8]);
	}

	ROREQ (extra_state, extra_state * 0x10);
	state[13] &= state[12] + 0x6b465da;

	if (a == 3) {
		mix_minor27;
		state[8] += 0x370c574;
		state[0] += 0xc484fc90;
		mix_major13 (state, state[11]);
	}

	state[16] |= state[14] + 0xff7068bf;
	state[7] &= state[19] ^ 0x1e569f2b;
	state[12] += state[15] * 0x49f90b6a;

	if (a == 6) {
		state[17] ^= 0x8ade6faa;
		mix_minor26;
		mix_minor59;
		mix_major24 (state, state[7]);
	}

	state[6] -= state[18] * 0xb0223a7;
	state[19] -= state[4] * 0x4f4bc59;
	state[17] += state[3] + 0x19da7ccb;
	state[17] -= extra_state & 0x3a423827;

	if (a == 9) {
		mix_minor67;
		mix_minor29;
		state[3] += 0x506840;
		mix_major23 (state, extra_state);
	}

	extra_state += state[11] + 0xea268d79;
	extra_state ^= state[11] + 0x7b41453;

	if (a == 8) {
		state[11] += 0xe199e061;
		mix_minor34;
		mix_minor30;
		mix_major1 (state, extra_state);
	}

	state[0] ^= state[2] ^ 0x361eddb9;
	state[0] += extra_state + 0xc3201c46;
	ROREQ (state[4], state[4] + 0x19);
	state[8] *= state[16] + 0xf6c0ea7;
	ROREQ (state[11], state[18] * 0x13);
	state[2] |= state[4] | 0x5747f7c;
	extra_state ^= state[3] * 0x336a3c4f;
	state[9] ^= state[8] + 0x5ff3732;
	state[9] ^= extra_state + 0x2b702a62;
	state[1] *= state[1] + 0xfa4e2f52;
}

void mix_major1 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[6] ^ state[9] ^ state[12]) % 11;
	state[5] += state[3] & 0x24398ab;
	extra_state += state[3] - state[18] + 0x45e6c9d4;

	if (a == 2) {
		mix_minor26;
		mix_minor58;
		mix_minor67;
		mix_major5 (state, state[19]);
	}

	extra_state ^= state[14] + 0xc0fd80ba;
	state[12] -= state[1] * 0xe99b672;
	state[15] ^= state[0] + 0xca70bf60;

	if (a == 1) {
		state[4] ^= 0x15e7d1d6;
		mix_minor44;
		mix_minor26;
		mix_major3 (state, state[11]);
	}

	extra_state += extra_state ^ 0x17339c6;
	state[15] += state[7] * 0x15f0a011;
	state[4] &= state[17] + 0x1b597286;
	state[17] *= state[15] & 0x389e630b;

	if (a == 3) {
		mix_minor44;
		mix_minor25;
		mix_minor63;
		mix_major14 (state, state[3]);
	}

	state[18] ^= state[19] ^ 0x31a138ce;
	state[16] &= extra_state * 0x271fe1f1;
	ROREQ (state[7], state[16] ^ 0x9);

	if (a == 5) {
		state[12] += 0x108440;
		state[14] -= 0xf9b7e88d;
		mix_minor64;
		mix_major9 (state, extra_state);
	}

	state[2] *= ROR (state[6], 31);
	extra_state -= state[14] - 0xfee822a8;
	extra_state *= state[5] * 0x9dfbe4;
	extra_state += state[13] + 0xfd2ead2f;

	if (a == 9) {
		mix_minor60;
		mix_minor42;
		state[14] += 0x723398ff;
		mix_major10 (state, state[18]);
	}

	state[7] += state[17] + 0x2b29baf9;
	ROREQ (state[2], ROL (state[0], 25));

	if (a == 6) {
		mix_minor46;
		ROLEQ (state[19], 18);
		mix_minor41;
		mix_major2 (state, state[4]);
	}

	state[12] &= state[16] + 0x2223fa4b;
	extra_state -= state[5] * 0x282f40d5;
	extra_state &= ROR (state[18], 16);

	if (a == 8) {
		mix_minor46;
		state[9] += 0xd0b27d9c;
		ROLEQ (state[10], 22);
		mix_major7 (state, extra_state);
	}

	state[17] += state[7] + 0xf9ac8515;
	state[7] += state[10] + 0xf9b69577;

	if (a == 4) {
		state[3] *= 0x2da1cfcf;
		mix_minor54;
		state[12] += 0x80410;
		mix_major21 (state, state[13]);
	}

	state[7] += state[13] ^ 0x6d56f7f;
	state[8] -= extra_state - 0x8c8d3d9c;

	if (a == 7) {
		mix_minor24;
		mix_minor33;
		mix_minor42;
		mix_major13 (state, state[4]);
	}

	state[5] -= state[12] - 0x4d2bd380;
	state[1] -= extra_state - 0xfcee8aad;
	state[18] *= state[1] * 0x696c0;
	state[8] *= state[4] + 0xdc2745dc;

	if (a == 10) {
		mix_minor58;
		mix_minor25;
		mix_minor36;
		mix_major24 (state, state[2]);
	}

	state[18] ^= state[7] + 0xd9de0ed7;
	ROREQ (state[11], state[6] + 0x11);
	state[19] += state[18] + 0xb295dc;

	if (a == 0) {
		mix_minor41;
		mix_minor30;
		mix_minor64;
		mix_major23 (state, extra_state);
	}

	state[15] -= extra_state | 0x58eafd;
	ROREQ (state[5], state[12] * 0x6);
	state[2] += state[19] + 0xf42fd441;
	state[12] *= state[2] | 0x10d913b8;
	state[1] ^= state[11] + 0x2039d1f9;
	state[15] += ROR (state[2], 3);
	state[11] += state[1] + 0x55f96491;
	state[4] *= state[15] & 0x864fe18;
	state[18] *= state[18] + 0xf5eb4571;
}

void mix_major2 (u32 *state, u32 extra_state)
{
	int a;

	a = state[9] % 11;
	state[0] |= extra_state | 0x4d9f89df;
	extra_state -= extra_state & 0x10691818;
	state[15] &= ROR (state[15], 18);

	if (a == 2) {
		mix_minor63;
		mix_minor62;
		mix_minor43;
		mix_major12 (state, state[9]);
	}

	extra_state |= state[7] ^ 0x1f11181f;
	ROREQ (state[17], state[18] + 0x18);
	state[3] &= state[18] + 0xc18379a4;
	state[8] += state[2] + 0x8845990;

	if (a == 8) {
		ROLEQ (state[10], 6);
		mix_minor64;
		mix_minor69;
		mix_major18 (state, extra_state);
	}

	extra_state *= ROL (state[7], 30);
	ROREQ (state[14], extra_state ^ 0x1);
	state[3] -= state[3] ^ 0x1a11c1c;

	if (a == 3) {
		mix_minor48;
		state[3] -= 0x833e3d40;
		mix_minor61;
		mix_major6 (state, extra_state);
	}

	state[5] += extra_state + 0xbdf50793;
	extra_state -= state[6] ^ 0x341c6ce5;
	extra_state ^= state[14] | 0x11712ba;
	state[4] -= extra_state - 0x1df0f08c;

	if (a == 4) {
		mix_minor44;
		mix_minor41;
		mix_minor64;
		mix_major4 (state, state[2]);
	}

	extra_state *= state[15] + 0xd8a810b1;
	state[0] -= state[7] - 0x8e4e3c5;
	state[9] -= extra_state ^ 0x13f1a8da;

	if (a == 7) {
		mix_minor20;
		mix_minor47;
		mix_minor57;
		mix_major11 (state, state[12]);
	}

	state[14] ^= extra_state + 0xf2dd8a98;
	state[14] |= state[3] & 0xb51383c;

	if (a == 1) {
		mix_minor36;
		mix_minor27;
		mix_minor47;
		mix_major22 (state, state[9]);
	}

	state[0] -= state[2] - 0x16bda446;
	state[2] -= state[0] ^ 0x3576dfb9;

	if (a == 9) {
		mix_minor26;
		mix_minor32;
		mix_minor49;
		mix_major5 (state, state[9]);
	}

	state[5] -= state[6] | 0x1720cf3;
	state[16] ^= state[19] ^ 0x2dfed60;
	extra_state *= state[12] + 0xffcf5d22;
	extra_state += state[11] ^ 0x26b4296;

	if (a == 6) {
		mix_minor55;
		ROLEQ (state[19], 15);
		ROLEQ (state[10], 26);
		mix_major3 (state, extra_state);
	}

	ROREQ (extra_state, ROL(extra_state, 11));
	extra_state -= ROL (state[17], 25);
	state[4] += state[3] ^ 0x125c14db;

	if (a == 0) {
		state[14] += 0x7de14a07;
		state[4] *= 0x13ca26ac;
		mix_minor41;
		mix_major14 (state, state[9]);
	}

	state[9] |= ROR (state[15], 31);
	extra_state -= state[19] - 0xfde54451;
	ROREQ (state[9], state[11] | 0x3);
	state[18] ^= extra_state ^ 0x22da8ee3;

	if (a == 10) {
		mix_minor46;
		mix_minor28;
		mix_minor69;
		mix_major9 (state, state[2]);
	}

	ROREQ (state[2], state[9] + 0xf);
	ROREQ (state[5], state[18] & 0x13);
	state[5] -= state[1] ^ 0x2822999;

	if (a == 5) {
		state[12] += 0x108072;
		state[8] += 0xaf45f1d7;
		mix_minor61;
		mix_major10 (state, state[9]);
	}

	state[1] += ROR (extra_state, 24);
	state[6] += state[4] | 0x161d3ea;
	state[9] += extra_state + 0xc2e590c;
	extra_state -= extra_state ^ 0x125deacd;
	state[7] &= state[17] ^ 0x10b015bf;
	state[17] = 0x1bb396c0;
	state[9] *= state[1] & 0x7a04e3e;
	extra_state += state[16] | 0x16cf1fa2;
	extra_state *= state[14] * 0x1d5ac40e;
	state[4] ^= extra_state + 0xf27819a7;
}

void mix_major3 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[10] ^ state[16] ^ extra_state) % 11;
	state[12] *= state[3] & 0x19997dc0;
	extra_state |= state[0] + 0xd31e211;
	state[14] -= state[0] - 0x7cfa160;

	if (a == 10) {
		mix_minor48;
		mix_minor46;
		state[14] -= 0x7f80fb4a;
		mix_major17 (state, state[2]);
	}

	state[13] = ROR (state[13], extra_state + 0x6);
	state[3] *= state[12] + 0xfd1d773c;

	if (a == 3) {
		mix_minor37;
		mix_minor39;
		state[17] ^= 0x1d4f264d;
		mix_major16 (state, state[12]);
	}

	extra_state |= extra_state + 0xd10c7a44;
	extra_state += state[0] + 0xf3754e81;
	extra_state ^= state[16] ^ 0x21d2a427;

	if (a == 1) {
		mix_minor38;
		state[12] += 0x208846a;
		mix_minor31;
		mix_major15 (state, state[11]);
	}

	state[16] |= state[4] | 0x599c0b2;
	ROLEQ (extra_state, state[0] + 0x1d);
	state[3] &= state[6] ^ 0x1d86d59a;
	state[0] ^= state[10] ^ 0x22d79e78;

	if (a == 9) {
		mix_minor45;
		ROLEQ (state[16], 14);
		mix_minor45;
		mix_major8 (state, state[16]);
	}

	ROLEQ (state[15], state[9] + 0x2);
	extra_state += ROL (extra_state, 13) + (state[4] ^ 0x17568f8b);
	state[3] -= state[9] ^ 0x1b7d211b;

	if (a == 7) {
		mix_minor35;
		state[14] ^= 0x7adc7a3f;
		mix_minor64;
		mix_major12 (state, extra_state);
	}

	state[14] *= state[10] ^ 0x25da4024;
	state[3] += state[19] ^ 0x195596e2;

	if (a == 8) {
		mix_minor37;
		mix_minor60;
		state[8] -= 0x75c7234e;
		mix_major18 (state, extra_state);
	}

	state[3] ^= ROR (state[4], 11);
	state[19] ^= state[2] & 0x142c74fa;
	state[7] = 0x3de4cf2b;
	extra_state ^= state[5] * 0x1195dbf3;

	if (a == 5) {
		mix_minor51;
		mix_minor51;
		mix_minor28;
		mix_major6 (state, state[9]);
	}

	state[12] *= state[14] * 0x25bf72d4;
	extra_state += ROL (state[11], 2);

	if (a == 0) {
		mix_minor28;
		mix_minor20;
		mix_minor41;
		mix_major4 (state, extra_state);
	}

	state[7] += extra_state + 0xfbd89057;
	state[12] -= extra_state - 0xfec898a3;
	extra_state *= extra_state + 0xe6d9d0ce;
	state[2] *= state[0] * 0x25d5927e;

	if (a == 6) {
		mix_minor41;
		mix_minor35;
		state[0] += 0x8a388c73;
		mix_major11 (state, state[3]);
	}

	extra_state -= extra_state ^ 0x7951f14a;
	extra_state *= state[19] ^ 0x159fa550;
	state[9] -= extra_state * 0x1b0d12a6;

	if (a == 4) {
		mix_minor58;
		mix_minor26;
		mix_minor26;
		mix_major22 (state, extra_state);
	}

	state[12] += state[4] | 0xf2ff1db;
	state[12] ^= state[2] & 0xac8676c;
	state[7] -= extra_state * 0x1a41598b;
	state[17] *= state[14] & 0x36ff2c0;

	if (a == 2) {
		mix_minor60;
		mix_minor67;
		mix_minor44;
		mix_major5 (state, state[11]);
	}

	state[19] ^= state[11] + 0xe311654d;
	state[18] ^= state[16] * 0x1267cd78;
	state[16] &= extra_state ^ 0x1c8b2015;
	state[16] *= state[16] | 0xc26f29a;
	state[14] &= state[10] + 0xcec46d19;
	state[8] -= state[8] ^ 0xc03874d;
}

void mix_major4 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[2] ^ state[15] ^ extra_state) % 9;
	state[14] += state[15] + 0xd3892fe6;
	state[2] -= extra_state - 0xe600fde6;
	state[15] ^= state[4] + 0x385e38e;
	state[18] |= extra_state + 0xc6189f52;

	if (a == 7) {
		mix_minor43;
		mix_minor24;
		state[3] += 0x9302800;
		mix_major19 (state, state[1]);
	}

	state[11] &= state[14] + 0x8f6f81a9;
	ROLEQ (state[12], ROR (state[6], 14));
	extra_state -= ROL (state[8], 14);
	ROREQ (state[0], ROR (state[11], 1));
	state[0] += state[11] ^ 0x43cd4d14;

	if (a == 3) {
		mix_minor22;
		mix_minor48;
		state[4] *= 0x2a2e8718;
		mix_major20 (state, state[14]);
	}

	state[3] -= state[8] ^ 0x155c464;
	state[16] += state[0] + 0xf8d647b6;
	state[2] ^= state[4] ^ 0x11e3788d;

	if (a == 5) {
		state[5] += 0xc4115253;
		mix_minor51;
		mix_minor55;
		mix_major17 (state, state[10]);
	}

	extra_state |= ROL (extra_state, 11);
	state[5] &= extra_state ^ 0x16984b90;
	state[16] += ROR (extra_state, 29);
	state[0] += state[15] + 0xc3e56f16;

	if (a == 2) {
		mix_minor46;
		ROLEQ (state[16], 7);
		mix_minor40;
		mix_major16 (state, state[1]);
	}

	state[5] &= state[11] + 0xe57356e7;
	state[18] -= extra_state ^ 0x23f157f6;
	extra_state -= state[18] & 0x155b7cc8;

	if (a == 1) {
		mix_minor48;
		state[5] += 0x6d08d06;
		mix_minor50;
		mix_major15 (state, extra_state);
	}

	state[8] |= state[5] | 0x21496d22;
	extra_state -= state[18] - 0x93b1543f;
	state[14] *= extra_state * 0x1db47609;
	ROREQ (state[7], state[10] ^ 0x1a);
	ROLEQ (state[7], state[18] + 0x1c);

	if (a == 0) {
		mix_minor46;
		ROLEQ (state[10], 4);
		mix_minor59;
		mix_major8 (state, extra_state);
	}

	state[8] ^= ROL (state[5], 3);
	state[6] ^= extra_state ^ 0x2c8ca15;
	state[13] += ROL (extra_state, 13);

	if (a == 4) {
		mix_minor61;
		state[3] *= 0x6c0de9fa;
		mix_minor34;
		mix_major12 (state, state[18]);
	}

	state[17] ^= state[2] & 0xa0962e5;
	state[3] *= extra_state & 0xd505f52;
	extra_state -= state[15] ^ 0x15284f42;

	if (a == 8) {
		mix_minor37;
		mix_minor47;
		state[12] += 0x2108058;
		mix_major18 (state, state[8]);
	}

	state[7] &= state[2] + 0xf8df2963;
	state[6] *= extra_state * 0x256b9c9c;
	state[10] += state[1] | 0xda16d9b;
	state[9] *= state[5] ^ 0x28b62e0c;

	if (a == 6) {
		mix_minor35;
		state[14] ^= 0x8a0974b;
		mix_minor37;
		mix_major6 (state, extra_state);
	}

	state[12] ^= state[5] * 0x23779c9e;
	state[10] *= ROR (state[19], 29);
	state[0] ^= state[10] ^ 0x38a5f94;
	extra_state += state[15] + 0x1c82e95e;
	ROLEQ (state[9], state[5] ^ 0x1d);
	state[12] += extra_state + 0xc0e4fa7d;
	state[17] ^= state[7] ^ 0x141bbf98;
	state[9] ^= ROR (state[18], 6);
	state[4] -= state[13] & 0x2373fe39;
	state[19] += ROL (extra_state, 15);
}

void mix_major5 (u32 *state, u32 extra_state)
{
	int a;

	a = state[18] % 11;
	state[5] |= state[17] * 0x2e7a089;
	state[3] ^= state[13] + 0x1fef7de0;
	extra_state -= state[16] ^ 0x8338b85;

	if (a == 0) {
		state[3] += 0x1000800;
		state[3] += 0x9102040;
		mix_minor30;
		mix_major20 (state, state[11]);
	}

	extra_state *= 0x1cd19bfb;
	state[3] *= state[12] + 0x15bdbb56;
	state[11] ^= extra_state + 0x374580a7;
	state[10] += extra_state | 0x86941f3;

	if (a == 4) {
		mix_minor32;
		ROLEQ (state[10], 25);
		mix_minor50;
		mix_major17 (state, state[18]);
	}

	state[6] -= state[16] ^ 0x11119dd6;
	state[13] += state[18] + 0xcb82c76c;
	state[8] -= state[1] ^ 0x3b98ae58;

	if (a == 9) {
		mix_minor42;
		mix_minor64;
		mix_minor21;
		mix_major16 (state, state[1]);
	}

	state[17] ^= state[17] + 0xcfd5283;
	state[5] &= state[13] + 0x539ef62;
	state[11] &= state[14] ^ 0x639b87fe;

	if (a == 8) {
		mix_minor22;
		mix_minor59;
		state[14] += 0x73204792;
		mix_major15 (state, state[18]);
	}

	state[12] -= extra_state | 0x369e02e;
	state[6] *= state[12] + 0xf0544c52;
	extra_state += state[5] + 0x8dcb06;
	state[12] -= extra_state & 0x632ffca;

	if (a == 3) {
		state[5] += 0xc6ac8583;
		mix_minor41;
		state[3] += 0x9004000;
		mix_major8 (state, state[17]);
	}

	state[16] -= state[6] * 0x345114ef;
	ROREQ (state[10], state[11] * 0x10);
	state[0] += state[4] & 0x18b74e25;

	if (a == 7) {
		state[12] += 0x1a;
		mix_minor53;
		mix_minor47;
		mix_major12 (state, state[15]);
	}

	state[2] -= state[2] ^ 0x18f1b56;
	ROLEQ (state[19], state[13] + 0x6);

	if (a == 6) {
		state[3] *= 0x27d3a148;
		state[4] *= 0xa24016a8;
		state[14] -= 0x3a2c78cf;
		mix_major18 (state, extra_state);
	}

	ROREQ (extra_state, state[7] + 0x16);
	ROLEQ (extra_state, state[14] + 0x11);

	if (a == 5) {
		state[3] *= 0x3713ed22;
		mix_minor29;
		mix_minor59;
		mix_major6 (state, state[7]);
	}

	extra_state -= state[10] - 0xd26e6435;
	ROLEQ (state[8], state[13] ^ 0x15);
	state[1] += state[10] ^ 0x1da5a5e2;

	if (a == 2) {
		mix_minor45;
		mix_minor55;
		ROLEQ (state[16], 13);
		mix_major4 (state, state[10]);
	}

	state[7] |= extra_state * 0x1665683f;
	state[6] += state[17] + 0xd3198985;
	extra_state &= state[1] * 0xb2490cd;

	if (a == 1) {
		mix_minor50;
		mix_minor45;
		state[8] += 0x749a003b;
		mix_major11 (state, state[6]);
	}

	state[13] -= state[3] ^ 0x49caa386;
	state[5] -= state[7] - 0xca44ad;
	extra_state += state[14] | 0xce2b27d;

	if (a == 10) {
		mix_minor33;
		mix_minor22;
		state[8] -= 0x7a3a25c3;
		mix_major22 (state, state[11]);
	}

	state[15] += state[6] + 0x9f72b74b;
	state[16] -= state[3] - 0xaa1914c0;
	extra_state -= ROL (extra_state, 18);
	state[14] ^= state[9] ^ 0x7a9f2d9;
	state[19] &= ROL (state[3], 10);
	extra_state *= state[15] * 0xd49e9d9;
	state[4] += state[2] ^ 0xc52d715;
	state[15] *= state[11] * 0x300c07b6;
	state[4] ^= extra_state * 0x59c5268;
	state[7] -= extra_state - 0xf1ae26ce;
}

void mix_major6 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[3] ^ state[5] ^ state[18]) & 7;
	state[7] ^= state[5] ^ 0x3610ff4;
	state[18] ^= ROL (state[14], 19);
	ROREQ (state[15], state[10] + 0xe);
	extra_state ^= state[1] + 0xa89a8207;
	extra_state &= 0xecc2fa7d;

	if (a == 0) {
		mix_minor54;
		state[4] *= 0x5141d713;
		mix_minor62;
		mix_major19 (state, extra_state);
	}

	state[15] ^= state[0] * 0x19dd786;
	extra_state *= ROL (extra_state, 12);
	state[17] &= extra_state | 0x1249d1c;
	state[15] ^= state[8] + 0x5e67551f;
	extra_state += state[0] * 0x320ea6ec;
	extra_state ^= state[19] + 0xee10c43d;

	if (a == 1) {
		ROLEQ (state[19], 6);
		state[3] += 0x1600840;
		mix_minor41;
		mix_major20 (state, state[2]);
	}

	ROREQ (state[15], extra_state ^ 0x7);
	state[5] -= state[14] * 0x54cc1685;
	state[12] -= extra_state - 0xf7d8f2fa;
	state[5] -= state[10] - 0xf95da87e;
	extra_state ^= ROL (state[8], 18);

	if (a == 5) {
		mix_minor53;
		mix_minor23;
		mix_minor60;
		mix_major17 (state, state[16]);
	}

	state[19] += state[2] ^ 0x4983faaa;
	extra_state &= state[6] & 0x911ab6a;
	state[17] &= state[2] + 0xfbb4acd7;
	state[5] += state[13] + 0xf96465d3;
	ROLEQ (extra_state, state[2] | 0x19);
	state[9] += state[2] | 0x176f7fa2;

	if (a == 7) {
		mix_minor53;
		mix_minor67;
		mix_minor38;
		mix_major16 (state, extra_state);
	}

	ROREQ (state[4], extra_state + 0x10);
	extra_state |= state[6] ^ 0x1ae616e0;
	extra_state ^= state[15] * 0x7f034;
	ROREQ (state[14], state[2] + 0x3);

	if (a == 6) {
		state[14] -= 0xa630c9b5;
		mix_minor33;
		state[8] -= 0xa8a2e592;
		mix_major15 (state, extra_state);
	}

	state[12] -= state[10] & 0x1311b0aa;
	state[14] ^= extra_state + 0xf5736e40;
	state[17] += ROR (state[18], 15);
	extra_state ^= state[11] + 0x25e8d98c;
	ROLEQ (state[0], state[14] | 0x8);
	state[13] -= state[3] ^ 0x2a68c40c;

	if (a == 3) {
		mix_minor40;
		state[14] ^= 0x4e96c3d9;
		state[3] *= 0x7b9dddda;
		mix_major8 (state, state[3]);
	}

	state[12] += ROR (extra_state, 12);
	ROLEQ (state[7], state[6] & 0x9);
	ROLEQ (extra_state, extra_state * 0x4);
	ROREQ (extra_state, state[16] ^ 0x4);
	ROLEQ (state[6], ROL (state[1], 11));

	if (a == 4) {
		mix_minor31;
		mix_minor31;
		ROLEQ (state[16], 28);
		mix_major12 (state, state[13]);
	}

	state[14] += ROR (state[14], 9);
	state[3] *= state[13] & 0x24b1abab;
	state[3] -= state[12] - 0x10decc67;
	extra_state *= state[15] ^ 0x194903b4;

	if (a == 2) {
		mix_minor33;
		mix_minor48;
		mix_minor66;
		mix_major18 (state, state[14]);
	}

	state[15] *= extra_state * 0x2ed0158e;
	state[14] += state[3] + 0xc4d28c7c;
	state[11] -= state[18] ^ 0x3e1bda7e;
	state[2] *= ROL (state[13], 24);
}

void mix_major7 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[3] ^ state[6] ^ extra_state) % 11;
	state[8] += extra_state * 0x25d21c70;
	extra_state += ROR (state[13], 26);
	state[15] += ROR (state[0], 18);

	if (a == 1) {
		mix_minor45;
		ROLEQ (state[16], 24);
		ROLEQ (state[16], 18);
		mix_major18 (state, state[9]);
	}

	extra_state += state[4] ^ 0x214bbbb;
	ROLEQ (state[5], extra_state * 0x1d);
	state[17] -= state[18] | 0x1102e01a;
	state[19] += state[12] + 0xf1e0cc5a;

	if (a == 0) {
		state[4] *= 0x73b12006;
		mix_minor49;
		mix_minor43;
		mix_major6 (state, state[13]);
	}

	state[8] |= extra_state * 0x33ff2ce9;
	state[4] *= extra_state + 0x2fe45acf;
	state[3] ^= ROR (extra_state, 13);
	extra_state ^= state[12] & 0x2e2ac892;

	if (a == 7) {
		mix_minor59;
		mix_minor69;
		mix_minor41;
		mix_major4 (state, state[15]);
	}

	extra_state *= ROL (state[14], 1);
	extra_state ^= extra_state + 0x7a3b4f0e;
	state[5] += state[11] ^ 0x5f050ce6;

	if (a == 2) {
		mix_minor46;
		mix_minor45;
		mix_minor24;
		mix_major11 (state, state[17]);
	}

	state[9] -= state[11] & 0x524788df;
	extra_state += state[3] + ROR (state[17], 18) + 0x17b2d86;
	state[12] |= state[17] ^ 0xd2348b5;

	if (a == 4) {
		mix_minor60;
		mix_minor56;
		mix_minor64;
		mix_major22 (state, extra_state);
	}

	state[4] += state[0] ^ 0x3ca6760a;
	ROREQ (state[10], state[12] & 0x1e);
	state[12] -= extra_state ^ 0x32b59495;
	state[11] -= state[7] ^ 0xcc6cef3;

	if (a == 9) {
		mix_minor51;
		mix_minor51;
		mix_minor46;
		mix_major5 (state, state[6]);
	}

	state[18] -= extra_state ^ 0x42ce4263;
	state[8] ^= state[15] + 0xfc1ccf0a;
	state[4] *= state[2] + 0xdc6ebf0;

	if (a == 8) {
		mix_minor34;
		mix_minor33;
		mix_minor35;
		mix_major3 (state, state[19]);
	}

	state[14] ^= state[17] + 0x29e0bfe6;
	state[2] ^= state[0] + 0xc0a98770;
	state[6] += ROL (state[11], 15);

	if (a == 10) {
		mix_minor43;
		mix_minor38;
		ROLEQ (state[10], 8);
		mix_major14 (state, extra_state);
	}

	extra_state += state[18] - state[5] + 0xff5138a0;

	if (a == 6) {
		ROLEQ (state[19], 22);
		mix_minor32;
		state[14] ^= 0x3ccf037;
		mix_major9 (state, extra_state);
	}

	ROREQ (state[3], state[17] & 0xa);
	state[3] ^= state[7] * 0x36e7ec8;

	if (a == 3) {
		mix_minor41;
		state[17] ^= 0xeeea146c;
		mix_minor64;
		mix_major10 (state, extra_state);
	}

	ROREQ (state[10], state[19] * 0x19);
	state[14] *= state[12] + 0xd914afe4;

	if (a == 5) {
		mix_minor22;
		mix_minor57;
		mix_minor44;
		mix_major2 (state, state[18]);
	}

	state[8] -= state[7] ^ 0x1609874e;
	state[10] ^= state[4] | 0x1e171635;
	state[6] -= state[16] - 0x19b93371;
}

void mix_major8 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[5] ^ state[9] ^ state[19]) % 5;
	state[5] += state[12] ^ 0xb6b4743;
	extra_state *= state[12] + 0x221bed03;
	state[3] *= state[11] ^ 0x2663a394;
	extra_state |= state[4] ^ 0x4f1894;
	state[5] &= 0xad85e5da;
	state[17] &= extra_state + 0xd191e790;
	extra_state += state[1] * 0x1c634b75;

	if (a == 2) {
		mix_minor34;
		mix_minor25;
		state[8] -= 0x3dcc78c5;
		mix_major19 (state, state[1]);
	}

	state[3] -= state[6] ^ 0x1fdc8171;
	state[15] ^= state[14] * 0xdc63a30;
	ROLEQ (state[7], extra_state + 0x8);
	extra_state ^= state[8] + 0xe4fb2084;
	state[6] -= extra_state - 0xb6a8bfd8;
	extra_state *= ROR (state[6], 1);
	state[13] *= ROR (state[8], 31);
	state[18] ^= state[15] + 0xa969bc16;

	if (a == 1) {
		mix_minor25;
		ROLEQ (state[10], 14);
		mix_minor38;
		mix_major20 (state, state[2]);
	}

	ROREQ (extra_state, state[1] & 0x6);
	state[8] -= state[17] - 0xeba05ea0;
	state[16] += state[19] + 0xe8427306;
	state[16] ^= state[7] + 0x35f9fb28;
	state[13] += extra_state & 0x16076281;
	extra_state *= extra_state + 0xe43a6120;
	state[1] -= state[3] - 0xd94074d;

	if (a == 3) {
		state[4] *= 0xdccff951;
		mix_minor53;
		mix_minor41;
		mix_major17 (state, state[17]);
	}

	ROREQ (extra_state, state[18] + 0x5);
	state[6] += extra_state + 0x126c7192;
	state[4] &= state[9] ^ 0xe4c97d9;
	extra_state ^= extra_state + 0x5246092;
	state[14] += state[3] + 0x12466f7c;
	state[7] -= state[19] - 0xe724e487;
	extra_state -= state[2] - 0xfffcc68a;
	state[2] -= state[12] * 0xf8b6e25;

	if (a == 4) {
		mix_minor36;
		state[17] ^= 0x5f26a27b;
		state[14] ^= 0x77f49770;
		mix_major16 (state, state[12]);
	}

	ROLEQ (state[3], state[6] ^ 0x11);
	state[4] += extra_state & 0x3dd7da06;
	state[11] *= state[8] + 0xb6484f2a;
	extra_state ^= state[8] & 0x274e05b8;
	state[18] ^= state[5] + 0x263032a4;
	state[16] ^= extra_state + 0x1a70ff38;

	if (a == 0) {
		mix_minor37;
		mix_minor22;
		mix_minor45;
		mix_major15 (state, state[10]);
	}

	state[4] += extra_state + 0x4a83a932;
	ROLEQ (state[19], state[2] + 0x10);
	ROLEQ (state[0], state[19]);
	extra_state += extra_state ^ 0x1bb7cdc3;
	extra_state -= state[4] - 0xf1efd9b1;
	state[11] ^= state[1] | 0x64a30a;
	state[1] -= state[8] - 0x4cd3708;
	state[0] += extra_state + 0xf6d388b6;
	state[8] -= state[1] - 0x4b8444f;
	ROLEQ (state[7], ROL (extra_state, 9));
	ROREQ (state[17], state[10] + 0x1c);
}

void mix_major9 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[1] ^ state[15] ^ state[19]) % 11;
	state[19] |= state[18] + 0xe56713bc;
	state[12] |= state[8] + 0xefc639fe;

	if (a == 2) {
		mix_minor40;
		mix_minor26;
		mix_minor65;
		mix_major15 (state, extra_state);
	}

	state[4] ^= state[18] + 0xf20ff41d;
	ROLEQ (extra_state, extra_state + 0xb);

	if (a == 5) {
		mix_minor28;
		mix_minor35;
		ROLEQ (state[19], 20);
		mix_major8 (state, extra_state);
	}

	state[2] ^= ROR (extra_state, 1);
	state[10] *= extra_state + 0x3842b736;

	if (a == 4) {
		mix_minor21;
		mix_minor28;
		state[3] -= 0x524e81e6;
		mix_major12 (state, state[13]);
	}

	state[5] ^= state[4] ^ 0x224deca3;
	state[9] += state[15] & 0xe43bfd6;
	state[12] += state[18] | 0x24e2f424;

	if (a == 1) {
		mix_minor43;
		state[8] += 0x6afab397;
		state[11] += 0x573a6da7;
		mix_major18 (state, state[0]);
	}

	state[11] *= state[10] + 0xf0b1e409;
	state[9] *= state[5] + 0x13bcdf0b;
	state[5] += 0x2961fc0;
	state[6] *= state[11] + 0xe91b219c;

	if (a == 7) {
		mix_minor29;
		mix_minor41;
		mix_minor28;
		mix_major6 (state, 0xefc5f81f);
	}

	state[1] *= state[1] + 0xff4abdb4;
	extra_state = 0x6f850fff;
	state[13] += ROR (state[10], 27);
	state[10] += state[3] + 0xea05fa03;

	if (a == 10) {
		mix_minor24;
		mix_minor31;
		ROLEQ (state[16], 20);
		mix_major4 (state, extra_state);
	}

	state[19] = (state[19] + 0xe8b6d37d) - state[2];
	extra_state ^= state[12] * 0xa95c314;

	if (a == 8) {
		mix_minor64;
		state[4] ^= 0xa54ee16;
		mix_minor54;
		mix_major11 (state, state[11]);
	}

	extra_state += state[11] & 0x346472bf;
	extra_state &= state[15] * 0xbeb977c;
	extra_state += state[2] ^ 0x33dd726a;
	state[19] &= extra_state ^ 0x13220e;

	if (a == 6) {
		mix_minor63;
		mix_minor54;
		mix_minor43;
		mix_major22 (state, state[2]);
	}

	extra_state *= extra_state + 0x13a371f7;
	ROLEQ (state[0], extra_state * 0x2);
	ROLEQ (extra_state, state[15] * 0xf);
	state[12] += state[11] | 0x15477725;

	if (a == 3) {
		mix_minor56;
		state[14] ^= 0x66bd03a9;
		mix_minor55;
		mix_major5 (state, state[9]);
	}

	state[16] += state[8] + 0xb2878320;
	state[0] += state[11] * 0x128142d3;
	ROREQ (state[13], extra_state + 0x9);

	if (a == 0) {
		mix_minor33;
		mix_minor69;
		mix_minor58;
		mix_major3 (state, state[17]);
	}

	ROREQ (state[13], state[4] + 0x1a);
	extra_state |= extra_state + 0xb401ddcd;
	ROREQ (extra_state, state[16] + 0x17);
	extra_state += state[11] ^ 0x14302fce;

	if (a == 9) {
		mix_minor30;
		mix_minor47;
		state[14] -= 0x979badcc;
		mix_major14 (state, state[17]);
	}

	state[7] += state[2] & 0x2104615d;
	state[6] |= ROL (state[4], 21);
	state[16] -= extra_state * 0x144af0fa;
	extra_state ^= state[9] * 0x1d7178c2;
	extra_state *= 0x3564b1fd;
	state[16] -= ROR (extra_state, 11);
	state[8] ^= state[19] * 0x383ae479;
	state[11] += extra_state + 0xc4759a85;
	state[9] ^= state[11] + 0x35e01882;
	state[10] &= state[0] ^ 0x105d6dd1;
}

void mix_major10 (u32 *state, u32 extra_state)
{
	int a;

	a = state[5] % 11;
	state[17] ^= extra_state + 0x2277a712;
	state[19] *= state[8] + 0xe6c6654e;
	ROREQ (state[6], state[1] ^ 0x1b);

	if (a == 3) {
		state[8] += 0x8c1d03c3;
		state[4] ^= 0x112c3767;
		mix_minor43;
		mix_major8 (state, state[1]);
	}

	state[0] *= extra_state + 0x22e5f53d;
	state[6] -= state[14] - 0xf7f0c308;

	if (a == 6) {
		state[1] &= 0x548aed34;
		mix_minor33;
		mix_minor28;
		mix_major12 (state, state[2]);
	}

	extra_state += state[9] + 0xafa2e81;
	state[15] *= state[17] + 0xfd2839c0;
	state[14] -= state[6] - 0x30bd8dc6;
	state[2] += state[7] ^ 0x1edb75c4;

	if (a == 4) {
		mix_minor67;
		mix_minor49;
		mix_minor29;
		mix_major18 (state, extra_state);
	}

	state[2] = 0x2cfa7327;
	state[7] -= state[8] - 0xf2bf5a7;
	ROREQ (state[11], state[6] | 0x15);
	state[2] ^= ROL (state[10], 24);

	if (a == 2) {
		ROLEQ (state[19], 19);
		mix_minor41;
		mix_minor26;
		mix_major6 (state, extra_state);
	}

	state[16] ^= ROL (state[5], 29);
	ROLEQ (state[8], ROL (state[8], 19));

	if (a == 0) {
		state[8] += 0xabc0d876;
		state[1] &= 0x2002d891;
		mix_minor51;
		mix_major4 (state, extra_state);
	}

	state[13] *= extra_state & 0x9aee05b;
	ROLEQ (state[18], state[0] + 0x9);

	if (a == 5) {
		mix_minor37;
		mix_minor52;
		mix_minor65;
		mix_major11 (state, state[5]);
	}

	state[16] += extra_state + 0x15c7f2a;
	state[0] += state[8] | 0xc568bd;
	extra_state += ROR (state[11], 25);

	if (a == 10) {
		mix_minor29;
		state[14] += 0x7bef2ee1;
		mix_minor55;
		mix_major22 (state, extra_state);
	}

	state[11] &= state[0] | 0x3c992378;
	extra_state ^= state[2] ^ 0x1ebdf827;
	extra_state ^= state[16] & 0x1a8092b;
	state[4] ^= state[2] + 0xf6a7c14d;

	if (a == 7) {
		state[3] += 0x706840;
		state[3] += 0x1400840;
		mix_minor68;
		mix_major5 (state, state[5]);
	}

	extra_state |= state[1] + 0xbd4eb37a;
	extra_state *= state[15] ^ 0xe476c17;

	if (a == 9) {
		mix_minor41;
		state[14] += 0x52aaba85;
		mix_minor68;
		mix_major3 (state, state[19]);
	}

	state[0] -= state[4] & 0x55d63dde;
	state[14] += state[19] + 0xfa050d42;
	state[12] &= state[0] + 0x9ff4339;
	state[15] ^= state[12] + 0xccdc186;

	if (a == 8) {
		mix_minor25;
		mix_minor31;
		mix_minor43;
		mix_major14 (state, state[12]);
	}

	ROREQ (state[10], state[11] + 0x1b);
	state[5] ^= state[15] + 0x130fea4;
	extra_state ^= state[19] + 0xdf1438e7;

	if (a == 1) {
		mix_minor20;
		mix_minor54;
		mix_minor35;
		mix_major9 (state, extra_state);
	}

	state[11] += state[3] ^ 0x30f43d2;
	state[13] -= state[16] * 0x485950f;
	state[15] *= state[1] + 0xa295d0d;
	extra_state ^= state[0] * 0x68f4b257;
	state[12] &= state[8] + 0xe49d7359;
	state[7] -= state[2] * 0x16a7a0b6;
	extra_state &= state[13] + 0x18727e9f;
	state[14] &= ROL (extra_state, 3);
	state[19] -= state[6] ^ 0x13892cf5;
}

void mix_major11 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[3] ^ state[11] ^ state[17]) % 10;
	state[15] -= state[0] & 0x201c33b4;
	state[9] &= state[4] ^ 0x4b5700f;
	state[14] *= extra_state - (state[15] | 0x1f564f3c) + 0xfe30d470;

	if (a == 2) {
		mix_minor58;
		state[1] &= 0xdc0e2e53;
		mix_minor65;
		mix_major19 (state, state[1]);
	}

	state[3] ^= ROL (state[7], 28);
	extra_state = 0xb2363254;
	state[17] += 0x503fc4de;
	state[18] += state[1] * 0xf14c9c;

	if (a == 6) {
		mix_minor21;
		mix_minor28;
		mix_minor54;
		mix_major20 (state, state[5]);
	}

	state[3] *= state[0] + 0xaf4b1f37;
	state[11] *= state[11] + 0x1d1cbc4e;
	state[13] ^= state[1] + 0xf6c6f628;
	state[17] ^= state[3] + 0x7f863fa;

	if (a == 4) {
		state[3] -= 0x7d6e042a;
		mix_minor31;
		state[12] += 0x2048070;
		mix_major17 (state, extra_state);
	}

	state[11] += state[4] | 0x3b62a700;
	state[19] ^= 0xf3c3d3f0;
	ROREQ (extra_state, state[10] + 0xe);
	state[16] |= ROR (state[16], 10);
	state[7] *= state[11] * 0x5053948;

	if (a == 3) {
		mix_minor58;
		state[3] *= 0x34797b50;
		mix_minor69;
		mix_major16 (state, state[4]);
	}

	state[1] &= extra_state * 0x377e5649;
	state[18] += state[2] | 0x57a0b91;
	ROLEQ (state[7], extra_state + 0x7);
	state[4] -= ROL (state[7], 2);

	if (a == 0) {
		state[1] &= 0x49102e08;
		state[12] += 0x20e0400;
		mix_minor56;
		mix_major15 (state, state[18]);
	}

	extra_state *= extra_state + 0xfea6f980;
	state[18] += state[2] * 0x33aaef75;
	state[2] ^= state[12] + 0xda4bd31e;
	extra_state -= state[6] | 0x107e370;
	state[17] -= extra_state - 0x191504c;

	if (a == 9) {
		mix_minor48;
		state[4] ^= 0xccc8d5fc;
		mix_minor41;
		mix_major8 (state, state[14]);
	}

	state[3] += ROL (state[15], 7);
	state[12] -= state[10] - 0x18afd3db;
	state[5] += state[12] + 0x1392be9b;
	state[5] -= state[3] ^ 0xfd205d5;
	state[8] ^= extra_state ^ 0x9000ce9;

	if (a == 5) {
		mix_minor51;
		mix_minor24;
		mix_minor32;
		mix_major12 (state, state[19]);
	}

	ROREQ (state[19], extra_state + 0x19);
	ROREQ (state[11], state[19] * 0x10);
	extra_state ^= state[12] ^ 0x534576d7;
	ROLEQ (state[11], state[1] ^ 0x15);
	state[19] += state[9] * 0x12af9c5;

	if (a == 8) {
		mix_minor42;
		mix_minor60;
		mix_major18 (state, state[0]);
	}

	ROLEQ (state[10], extra_state * 0x14);
	state[1] -= ROL (state[14], 19);
	extra_state |= state[16] + 0xed222733;
	state[16] &= state[3] * 0x532f53a;
	extra_state ^= state[11] * 0x14718f9a;

	if (a == 1) {
		mix_minor49;
		mix_minor63;
		state[1] &= 0xc2c9d439;
		mix_major6 (state, state[13]);
	}

	state[3] *= extra_state | 0x1739a522;
	extra_state *= state[1] | 0x4b09e3e;
	state[7] ^= state[12] ^ 0x2a4ea48a;

	if (a == 7) {
		mix_minor35;
		mix_minor35;
		state[4] *= 0x9b2bcf2e;
		mix_major4 (state, extra_state);
	}

	state[19] -= extra_state - 0x1dc54aa;
}

void mix_major12 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[7] ^ state[16] ^ extra_state) % 6;
	state[18] &= state[6] & 0x104394c4;
	extra_state *= extra_state + 0xe92519e2;
	state[4] += state[19] + 0x46d5ad23;
	state[6] += state[1] + 0x3fd0884;
	extra_state = state[9] * (extra_state + 0xc46fe68);
	state[9] = extra_state;

	if (a == 5) {
		state[8] += 0xb0568904;
		mix_minor55;
		mix_minor56;
		mix_major19 (state, state[4]);
	}

	state[11] ^= state[7] ^ 0x4453b1d7;
	state[4] ^= state[12] + 0x187596ce;
	state[14] += state[19] ^ 0x1ecd4347;
	state[17] &= state[6] + 0xaa504a66;
	state[13] -= state[7] - 0x2482f7ba;

	if (a == 2) {
		ROLEQ (state[16], 27);
		state[3] += 0x8602040;
		mix_minor37;
		mix_major20 (state, state[18]);
	}

	state[5] *= state[17] | 0x14128b1f;
	state[5] &= state[9] | 0x8ae69ec;
	extra_state = (state[5] | 0x25dcee2a) * 0xf7abca44;
	state[12] += state[10] * 0x2b5c108a;
	state[19] -= state[10] - 0x45d1e08;

	if (a == 1) {
		mix_minor61;
		state[3] += 0x1704000;
		state[12] += 0x20e002a;
		mix_major17 (state, state[7]);
	}

	state[5] -= state[3] - 0x17a9626b;
	extra_state += state[8] + 0x55003f14;
	state[9] += ROL (state[6], 31);
	state[2] |= ROL (state[19], 13);
	state[19] ^= state[15] ^ 0xfbf02d6;
	state[3] |= state[18] * 0x279ed38c;
	extra_state &= state[19] ^ 0x234a2088;

	if (a == 0) {
		mix_minor42;
		state[12] += 0x68468;
		mix_minor55;
		mix_major16 (state, state[14]);
	}

	state[4] += state[9] + 0xd5555942;
	state[6] += state[0] + 0xf6a829d0;
	state[2] += state[17] * 0x6877a2b6;
	extra_state |= state[11] + 0x4f92882e;
	state[4] ^= extra_state + 0x2a0e1a7a;
	extra_state *= extra_state * 0xba88b94;

	if (a == 3) {
		state[14] += 0x5a9acc8f;
		mix_minor40;
		mix_minor33;
		mix_major15 (state, extra_state);
	}

	state[8] -= state[19] ^ 0x88fae5c;
	extra_state -= extra_state ^ 0x6171e1a;
	extra_state *= state[0] & 0x6369ab7c;
	state[2] ^= state[12] & 0x36b79ddb;
	extra_state ^= extra_state + 0xff3ba490;

	if (a == 4) {
		mix_minor42;
		mix_minor39;
		state[8] += 0x9cf399e7;
		mix_major8 (state, state[2]);
	}

	extra_state ^= state[9] * 0x2a0582f6;
	state[9] ^= state[10] + 0xf71f2197;
	state[17] |= extra_state + 0x417b0639;
	state[6] ^= ROR (extra_state, 17);
	state[15] -= state[3] - 0x1935355;
	state[13] += state[5] + 0x25393a1;
}

void mix_major13 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[1] ^ state[12] ^ state[18]) % 11;
	state[7] *= extra_state + 0xfd2296dd;
	extra_state *= state[9] + 0x10ce1e6b;
	state[13] |= state[14] & 0xe7aa887;

	if (a == 9) {
		mix_minor61;
		state[3] += 0x1702840;
		mix_minor34;
		mix_major4 (state, state[15]);
	}

	state[19] += state[17] + 0x44864e65;
	state[2] -= state[10] - 0x456501d3;
	state[11] ^= state[17] + 0xe91158ed;

	if (a == 6) {
		mix_minor41;
		mix_minor46;
		mix_minor27;
		mix_major11 (state, state[8]);
	}

	state[13] -= extra_state - 0xffeafe84;
	state[3] ^= state[10] & 0x5898bbff;
	extra_state -= state[17] ^ 0xb4b5ddd;
	state[5] &= extra_state + 0xf2a69347;

	if (a == 7) {
		state[11] += 0x28b81;
		mix_minor55;
		mix_minor38;
		mix_major22 (state, state[19]);
	}

	state[8] += state[11] + 0x35a3f082;
	state[15] &= extra_state + 0xf0918e1c;

	if (a == 8) {
		state[12] += 0x2180072;
		mix_minor48;
		mix_minor39;
		mix_major5 (state, extra_state);
	}

	extra_state -= state[12] - 0x1e87b29e;
	extra_state ^= state[0] + 0x9b993250;
	state[13] ^= state[17] * 0xb083b2b;

	if (a == 5) {
		mix_minor68;
		mix_minor58;
		mix_minor52;
		mix_major3 (state, state[14]);
	}

	ROLEQ (state[1], state[0] ^ 0x1a);
	state[5] ^= state[11] * 0x17321349;
	extra_state ^= state[3] + 0xffce689b;
	state[4] *= extra_state + 0x2570be6e;

	if (a == 10) {
		state[14] -= 0xb271fe0e;
		mix_minor57;
		mix_minor20;
		mix_major14 (state, state[6]);
	}

	state[15] *= 0x2d42b937;
	state[4] *= extra_state + 0xf544478e;
	state[0] += state[9] ^ 0x4dc36a;
	state[0] -= extra_state - 0x10bb4f25;

	if (a == 3) {
		mix_minor39;
		mix_minor26;
		state[0] += 0x8fc063b5;
		mix_major9 (state, state[15]);
	}

	state[19] &= ROR (state[3], 14);
	state[17] *= extra_state * 0x18575b09;
	state[1] |= extra_state * 0x50ebe77;
	extra_state += state[6] | 0x4d24003d;

	if (a == 4) {
		state[14] -= 0x3b677863;
		mix_minor29;
		mix_minor22;
		mix_major10 (state, state[9]);
	}

	state[15] &= state[0] + 0xf770857b;
	ROREQ (state[0], extra_state * 0xd);
	extra_state -= extra_state | 0x2576a843;

	if (a == 0) {
		mix_minor49;
		mix_minor41;
		state[3] += 0x8306000;
		mix_major2 (state, state[8]);
	}

	state[1] += extra_state * 0x2994c8c;
	state[16] ^= state[6] + 0xfe25a480;
	state[3] *= state[11] * 0x1e333f7b;
	ROREQ (state[7], state[17] ^ 0x1a);

	if (a == 2) {
		mix_minor51;
		state[8] -= 0xfbb3cb07;
		state[4] ^= 0x214ff68b;
		mix_major7 (state, state[1]);
	}

	state[13] ^= state[18] + 0x149e5b40;
	state[0] += state[19] + 0x541a494;

	if (a == 1) {
		mix_minor38;
		mix_minor28;
		mix_minor50;
		mix_major21 (state, extra_state);
	}

	state[2] -= extra_state - 0x16deeae;
	state[9] -= state[0] ^ 0x1120ce2d;
	state[13] ^= state[7] ^ 0x2a74ac2a;
	state[12] &= state[9] + 0xdab80c67;
	state[14] -= extra_state * 0x2776477;
	state[4] -= state[19] * 0x2f2e21d0;
	state[19] -= state[3] - 0xe78ae13d;
	extra_state -= extra_state ^ 0x434c0d3a;
	extra_state -= state[2] - 0x11f70706;
	ROREQ (extra_state, state[16] + 0x9);
	state[13] += extra_state * 0x2a0d21c3;
}

void mix_major14 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[6] ^ state[8] ^ state[15]) % 11;
	state[14] &= extra_state ^ 0x1c0b5143;
	state[17] *= state[5] + 0x4ef38b53;
	state[15] ^= ROR (state[16], 8);

	if (a == 4) {
		state[4] ^= 0x82254dc0;
		mix_minor36;
		mix_minor47;
		mix_major16 (state, state[10]);
	}

	extra_state ^= state[17] & 0x3b118c17;
	ROREQ (extra_state, state[7] * 0xb);
	state[5] -= ROR (state[12], 5);

	if (a == 10) {
		state[14] -= 0x7b59f866;
		state[3] -= 0x6bfee72c;
		state[3] += 0x1704040;
		mix_major15 (state, extra_state);
	}

	extra_state ^= state[10] + 0xe81a232b;
	state[18] |= state[2] + 0xef9e8d77;
	state[3] += state[4] + 0xce3d3234;

	if (a == 5) {
		mix_minor27;
		mix_minor38;
		mix_minor20;
		mix_major8 (state, state[0]);
	}

	extra_state *= ROR (extra_state, 15);
	extra_state &= state[7] + 0x358107b;
	state[12] += ROL (state[3], 20);

	if (a == 3) {
		mix_minor20;
		state[17] ^= 0xde7b4629;
		state[4] ^= 0x5cfc1b41;
		mix_major12 (state, extra_state);
	}

	extra_state += extra_state + 0xddcb6fb3;
	extra_state ^= state[4] * 0x2a5c35ea;
	state[4] -= state[3] - 0x3b4034a1;
	state[11] &= state[19] | 0x2856103;

	if (a == 1) {
		mix_minor21;
		mix_minor56;
		mix_minor25;
		mix_major18 (state, state[16]);
	}

	state[7] |= extra_state + 0x2d3d686;
	extra_state &= state[15] & 0x316de5b2;

	if (a == 7) {
		mix_minor20;
		mix_minor33;
		state[14] ^= 0x1e127778;
		mix_major6 (state, state[15]);
	}

	extra_state ^= state[17] ^ 0x3e8999a9;
	extra_state += extra_state + 0x4d77c09e;
	state[6] *= state[10] + 0xd1650ad7;
	state[7] *= state[3] & 0xade0835;

	if (a == 0) {
		mix_minor26;
		mix_minor28;
		mix_minor67;
		mix_major4 (state, state[3]);
	}

	state[9] -= state[15] ^ 0x32bd1767;
	state[12] ^= state[3] + 0x74289e8a;
	state[9] ^= state[5] + 0xd55d1b86;
	extra_state &= state[12] * 0x13b7b134;

	if (a == 8) {
		mix_minor35;
		mix_minor30;
		mix_minor21;
		mix_major11 (state, state[2]);
	}

	extra_state += extra_state + 0xda1b9ad7;
	state[6] -= state[18] * 0x452ad09;
	state[4] += extra_state ^ 0x4895c9e2;

	if (a == 9) {
		mix_minor49;
		mix_minor49;
		mix_minor46;
		mix_major22 (state, state[16]);
	}

	extra_state ^= extra_state + 0xf8ecf928;
	ROREQ (state[18], state[5] + 0xd);

	if (a == 6) {
		mix_minor55;
		mix_minor69;
		mix_minor67;
		mix_major5 (state, state[8]);
	}

	extra_state *= 0x34b70af0;
	state[5] -= ROL (state[19], 23);

	if (a == 2) {
		mix_minor46;
		mix_minor47;
		mix_minor47;
		mix_major3 (state, state[18]);
	}

	state[8] *= ROR (state[5], 2);
	state[17] += state[8] & 0x15595f;
	ROREQ (state[19], state[7] + 0x1);
	state[9] -= extra_state * 0x539f549;
	state[0] *= state[8] ^ 0x10549d01;
	state[11] -= state[4] ^ 0x1cd38676;
	state[12] += ROR (extra_state, 16);
	state[17] ^= state[15] + 0x266b587;
	state[17] -= ROR (state[0], 29);
	state[3] -= state[13] - 0x2669d0a1;
}

void mix_major15 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[12] ^ state[15] ^ extra_state) & 3;
	ROREQ (state[6], state[3] ^ 0x14);
	state[12] += extra_state ^ 0x9a94557;
	state[15] *= state[6] ^ 0x2c63c7d7;
	state[4] -= state[17] - 0x1565237b;
	ROLEQ (extra_state, state[11] * 0x19);
	extra_state -= state[9] * 0x3471499e;
	extra_state ^= state[3] ^ 0x34293622;
	state[11] += extra_state + 0xbab1970a;
	state[7] |= state[18] & 0x2e7cbf50;

	if (a == 2) {
		mix_minor28;
		mix_minor53;
		state[8] += 0xabdd8689;
		mix_major19 (state, state[11]);
	}

	state[16] &= state[12] + 0xc178e538;
	state[14] |= state[6] * 0xf7a199;
	state[9] += extra_state + 0x598a281;
	extra_state ^= state[0] + 0xf6c67dcd;
	state[14] += state[12] * 0x2a688905;
	ROREQ (state[16], extra_state | 0x9);
	state[10] += extra_state | 0x4d8cb855;
	state[19] -= state[9] - 0x32b94292;
	ROREQ (extra_state, state[9] * 0x9);

	if (a == 1) {
		state[1] &= 0xbe845151;
		mix_minor66;
		state[14] -= 0x77ab88ea;
		mix_major20 (state, extra_state);
	}

	state[6] &= ROL (state[10], 28);
	state[16] += extra_state ^ 0x5aafcd4a;
	state[12] &= extra_state ^ 0x1c22a3b7;
	state[18] -= state[4] * 0x358b021d;
	state[16] ^= state[13] + 0xac30f7a;
	ROLEQ (extra_state, state[17] ^ 0xe);
	ROREQ (extra_state, state[1] + 0x2);
	state[18] -= extra_state - 0xee6e38da;

	if (a == 0) {
		mix_minor31;
		mix_minor31;
		mix_minor52;
		mix_major17 (state, state[9]);
	}

	state[2] += state[16] | 0x5cbeb00;
	state[7] -= ROR (extra_state, 22);
	state[4] ^= extra_state + 0x1580fb54;
	state[17] -= ROL (state[12], 25);
	state[16] += state[8] ^ 0x1b3ea2;
	state[5] -= extra_state - 0x193cf230;
	ROLEQ (state[18], extra_state + 0x12);
	extra_state -= state[17] & 0x66e0e812;
	state[12] ^= ROL (state[7], 18);
	state[17] -= state[13] - 0xb70d1a;

	if (a == 3) {
		mix_minor46;
		state[1] &= 0x24c41868;
		mix_minor24;
		mix_major16 (state, state[17]);
	}

	state[6] += state[1] + 0xdfef3914;
	extra_state += ROL (state[5], 29);
	state[18] -= state[8] | 0x456bd4b;
	extra_state &= state[13] + 0x123e07ad;
	state[0] ^= extra_state * 0x22af60a0;
	state[13] -= state[12] - 0xf69f7aa2;
	ROREQ (state[17], extra_state ^ 0x1c);
	state[13] += state[5] * 0x248bf14b;
	state[2] ^= ROR (extra_state, 12);
}

void mix_major16 (u32 *state, u32 extra_state)
{
	int a;

	a = state[12] % 3;
	state[7] ^= state[7] + 0x1256f342;
	state[9] ^= ROL (state[14], 9);
	state[0] += state[13] ^ 0x4a20925;
	ROREQ (state[13], extra_state | 0xb);
	extra_state -= state[10] - 0x2cd8307e;
	ROREQ (extra_state, state[17] * 0x15);
	state[8] += state[15] | 0x11570bca;
	extra_state &= state[3] ^ 0x4c404c71;
	extra_state += state[10] ^ 0x85d82e;
	state[11] *= state[6] & 0xf076b8f;
	state[11] += extra_state + 0x26d0f98c;

	if (a == 0) {
		mix_minor29;
		mix_minor21;
		mix_minor33;
		mix_major19 (state, state[7]);
	}

	state[1] ^= ROR (extra_state, 23);
	state[9] += extra_state + 0xf24cc80b;
	ROLEQ (state[3], state[14] * 0x1d);
	state[19] += extra_state + 0x64922cc;
	extra_state -= state[0] - 0x1e0944e3;
	ROREQ (extra_state, extra_state * 0x1c);
	extra_state *= state[15] + 0x8d90c5a3;
	state[17] ^= extra_state & 0xdd9bf1a;
	state[4] -= state[6] - 0xd5bd8bc1;
	extra_state += extra_state + 0x1226f462;
	state[17] ^= ROL (state[13], 5);
	ROLEQ (state[13], extra_state & 0x12);

	if (a == 2) {
		mix_minor45;
		state[8] -= 0x3e5f74f5;
		state[11] += 0xee0e47c6;
		mix_major20 (state, extra_state);
	}

	extra_state |= state[9] | 0x10b9b57a;
	state[0] += state[10] + 0x477a65c2;
	state[8] |= state[7] ^ 0x1b348ba1;
	ROLEQ (state[16], ROL (state[1], 8));
	extra_state ^= state[19] * 0xfa375c5;
	ROREQ (state[11], ROR (state[5], 13));
	state[7] ^= state[19] + 0x64bd3f85;
	state[6] *= ROR (extra_state, 25);
	state[5] += extra_state + 0xaeeb67de;
	state[19] |= ROR (state[5], 22);
	state[0] += state[6] + 0xe1f2872;

	if (a == 1) {
		mix_minor53;
		mix_minor27;
		state[9] += 0xd829ce84;
		mix_major17 (state, extra_state);
	}

	extra_state |= state[6] | 0x40c95dca;
	ROLEQ (extra_state, state[12] ^ 0x1);
	state[3] &= state[8] + 0xed5ca98b;
	state[4] += extra_state + 0x92abec6e;
	extra_state &= ROR (state[13], 22);
	state[2] += state[15] * 0xff635ec;
	state[6] ^= extra_state + 0x37343841;
	state[9] += state[14] + 0xf8e12c69;
	state[14] -= ROL (state[10], 20);
}

void mix_major17 (u32 *state, u32 extra_state)
{
	int a;

	a = extra_state & 1;
	state[5] -= state[18] - 0x34b87873;
	state[17] -= state[1] - 0x2051ec4;
	state[6] ^= state[16] ^ 0x5c80bc7;
	extra_state -= ROR (extra_state, 26);
	state[5] *= state[16] | 0x154e9813;
	state[0] |= state[5] + 0xbac2a47e;
	state[13] *= state[9] ^ 0xbf263a6;
	state[9] |= ROL (state[11], 23);
	state[16] *= state[1] & 0x1c28de84;
	state[6] ^= ROR (state[2], 11);
	state[12] ^= ROR (state[9], 24);
	extra_state += extra_state + 0x2c5a0200;
	state[19] |= state[12] + 0xa104f7f6;
	state[17] ^= state[11] + 0xf51e9043;
	state[15] += extra_state + 0x37f1bc89;

	if (a == 0) {
		state[3] -= 0x2ae49a0;
		state[9] += 0xde755696;
		mix_minor53;
		mix_major19 (state, state[4]);
	}

	extra_state += state[5] | 0x79ba9a48;
	state[4] -= state[2] ^ 0x1ecdadba;
	extra_state ^= state[10] + 0xf01ca4cf;
	extra_state ^= state[8] + 0xf58222aa;
	state[8] |= state[7] * 0x59c62257;
	state[7] ^= state[7] | 0x2d2750f0;
	extra_state += state[17] | 0x1719d4f;
	state[19] *= state[4] + 0xcec35bec;
	state[18] ^= state[2] + 0xdc17a237;
	state[19] += state[5] + 0xca0f8bc5;
	extra_state += extra_state + 0xff282d98;
	state[0] += extra_state + 0x2a09f2a5;
	state[11] ^= state[2] + 0x30e437d6;
	state[12] |= extra_state + 0xee36df26;
	state[15] &= extra_state + 0xc95e1442;

	if (a == 1) {
		mix_minor27;
		state[9] += 0xd68c597b;
		state[9] += 0xdcb2dc4d;
		mix_major20 (state, state[14]);
	}

	state[7] -= state[17] ^ 0x72eeed7;
	state[17] *= state[15] * 0x162a030d;
	state[7] &= state[14] + 0xf0dd3ef3;
	extra_state += state[1];
	state[2] ^= state[13] ^ 0x2d9ceb17;
	state[7] &= extra_state ^ 0x176b1b8e;
	state[8] |= extra_state + 0xdab13e76;
	state[16] -= state[12] - 0x2a74b8d4;
	extra_state -= state[4] - 0xcc1039a3;
	extra_state -= state[5] * 0x1239378b;
	state[0] ^= extra_state ^ 0xd9a5ac4;
	state[10] -= state[1] ^ 0x346ff630;
	state[14] += state[15] ^ 0x2f99340b;
	state[11] |= state[7] + 0xd5881b85;
	ROLEQ (state[9], state[16] * 0x19);
}

void mix_major18 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[13] ^ state[16] ^ state[17]) % 7;
	state[2] -= state[9] - 0xe7e9ac84;
	state[7] &= extra_state + 0xd5e47036;
	state[7] ^= state[18] ^ 0x5d5e7006;
	extra_state += state[6] ^ 0x16afd25f;
	ROREQ (state[0], state[18] | 0x1b);

	if (a == 4) {
		mix_minor33;
		ROLEQ (state[16], 12);
		mix_minor39;
		mix_major19 (state, state[17]);
	}

	state[1] *= state[0] * 0x927384a;
	extra_state ^= state[6] * 0x2ac0b63c;
	extra_state ^= state[5] * 0xef44412;
	extra_state -= ROL (state[18], 22);

	if (a == 1) {
		mix_minor37;
		mix_minor23;
		mix_minor35;
		mix_major20 (state, extra_state);
	}

	state[6] &= extra_state + 0x4d05da6a;
	state[13] *= state[18] ^ 0xe2ba11c;
	extra_state ^= state[2] ^ 0x2e3d328f;
	extra_state *= state[1] | 0x110c8a1;
	ROLEQ (state[4], ROR (state[6], 27));

	if (a == 0) {
		state[3] -= 0xab85f363;
		mix_minor47;
		ROLEQ (state[10], 12);
		mix_major17 (state, extra_state);
	}

	state[19] &= ROR (extra_state, 8);
	extra_state |= ROR (state[19], 12);
	extra_state += state[14] * 0x2d8924b3;
	state[10] ^= state[15] + 0xdcba6126;
	extra_state += state[16] & 0xf72e29a;
	state[3] -= state[18] | 0x7614cfb;

	if (a == 6) {
		mix_minor60;
		state[3] *= 0x23a0356c;
		mix_minor33;
		mix_major16 (state, state[9]);
	}

	state[19] &= state[4] + 0xfe6ea18f;
	state[6] *= state[7] & 0x226185b2;
	state[0] += state[4] ^ 0x35388017;
	extra_state ^= state[14] * 0x268d6eae;

	if (a == 3) {
		state[14] += 0x72559385;
		state[8] += 0xafa7ed31;
		mix_minor26;
		mix_major15 (state, state[0]);
	}

	state[15] += extra_state ^ 0xbf3b8c0;
	ROREQ (state[10], ROR (state[18], 25));
	state[19] |= extra_state ^ 0x61d2180;
	state[4] &= state[19] + 0x588d79a3;

	if (a == 5) {
		state[11] += 0xa26a5e66;
		state[9] += 0xcdf889ea;
		mix_minor69;
		mix_major8 (state, state[8]);
	}

	state[0] += extra_state + 0x19039f88;
	ROLEQ (extra_state, ROR (state[7], 14));
	state[6] += state[8] ^ 0x1f3dce4;
	state[17] *= state[18] + 0x4f2cb877;
	state[6] &= state[15] * 0x177f5d63;
	ROLEQ (state[12], ROL (state[16], 1));

	if (a == 2) {
		mix_minor23;
		mix_minor32;
		state[9] += 0xc3b96ef0;
		mix_major12 (state, state[18]);
	}

	state[19] += state[12] + 0xbe9fd027;
	extra_state &= state[2] * 0x3ec8c5cb;
	state[8] += state[4] & 0x48357b75;
	ROLEQ (state[1], state[6] + 0x14);
	state[14] ^= state[11] + 0x13c7dc0f;
	state[4] += ROL (extra_state, 19);
	state[12] -= state[2] - 0x15ea2e80;
	state[11] += state[19] + 0xaff84c32;
	state[2] ^= state[5] * 0x278991a8;
	state[14] += state[2] + 0xf431b0d4;
}

void mix_major19 (u32 *state, u32 extra_state)
{
	state[3] ^= extra_state + 0xd2670e69;
	extra_state ^= state[2] & 0x3bd91a6d;
	extra_state += state[12] + 0xe162a863;
	state[3] -= extra_state - 0x2f72a89a;
	state[11] ^= state[3] & 0x4053f57a;
	extra_state *= state[12] + 0xfe64a9df;
	state[6] ^= state[14] ^ 0x6c235a3;
	ROLEQ (state[11], state[7] + 0x1b);
	state[13] ^= state[8] + 0xdf869976;
	ROLEQ (state[4], state[12] + 0x1);
	state[9] += state[13] ^ 0x3d475dc2;
	state[14] += ROR (extra_state, 25);
	state[10] += extra_state ^ 0x222fef6f;
	state[12] ^= ROR (state[19], 9);
	state[4] += state[8] + 0x56d964ed;
	state[18] -= state[7] - 0x132444b;
	state[0] = 0xd35add1b;
	extra_state += state[6] * 0x6fe2b2f;
	state[14] *= state[1] + 0xacf6925;
	state[15] ^= ROR (extra_state, 19);
	state[0] = 0xc0c8f110 - state[15];
	extra_state ^= extra_state | 0x2a57ebeb;
	state[12] -= ROL (extra_state, 13);
	extra_state |= state[13] & 0x15a66bda;
	extra_state += state[18] + 0x235ac102;
	ROLEQ (state[8], extra_state + 0xd);
	state[5] ^= state[12] + 0x3bbb70fe;
	extra_state &= state[1] + 0xec51134a;
	state[19] ^= state[11] * 0x87095a6;
	state[0] -= state[5] & 0xf43f6fb;
	state[15] += state[4] + 0xea66f8dc;
	state[19] -= state[7] - 0xd049cfd6;
	state[13] -= state[0] ^ 0x253c86f9;
	extra_state += state[16] | 0x520e84ba;
	state[10] -= ROL (state[18], 23);
	state[18] += extra_state * 0x2ee5918a;
	state[7] &= state[11] ^ 0xf0a32bc;
	state[0] -= extra_state ^ 0xfa89177;
	state[2] |= ROL (state[9], 24);
	state[14] *= 0x1cb1574a;
	ROREQ (state[10], extra_state + 0xd);
	state[1] &= state[11] + 0xef291170;
	state[6] += state[19] & 0x259a6745;
	extra_state -= state[2] ^ 0x10467b8;
	state[18] *= state[9] + 0xdbff9c2b;
	state[16] += extra_state ^ 0x8d4c279;
	state[5] -= ROR (state[10], 24);
}

void mix_major20 (u32 *state, u32 extra_state)
{
	ROLEQ (state[14], extra_state & 0xe);
	extra_state += state[15] ^ 0xbf446ce;
	extra_state += extra_state + 0xe227ea76;
	state[19] *= state[15] * 0x50ee813;
	ROREQ (state[19], state[10] + 0x1e);
	state[16] -= extra_state & 0x372035b;
	state[18] |= state[5] + 0xd9d1da08;
	state[5] += ROL (state[11], 9);
	ROREQ (state[8], state[0] * 0x3);
	extra_state += state[0] ^ 0x46d0b40;
	state[14] ^= state[1] + 0xe8684fc;
	extra_state -= state[14] * 0x28f80128;
	extra_state ^= ROR (state[1], 10);
	state[19] *= state[16] + 0xa0397f;
	state[13] += ROL (extra_state, 17);
	state[4] -= state[6] - 0x95670090;
	ROLEQ (state[14], 24);
	state[0] *= 0xe6219a29;
	state[3] += state[11] + 0xfa61efff;
	state[8] -= state[10] - 0xda64c153;
	state[9] -= state[8] - 0x22a4da90;
	extra_state = ROL (0xf2eafbc6, state[12] ^ 0x15);
	extra_state ^= state[19] | 0x2cd48d0d;
	extra_state += state[19] + 0xc6a5343a;
	state[13] -= extra_state - 0xc3172899;
	state[14] ^= ROL (state[4], 11);
	state[19] ^= state[14] ^ 0x274bf2e7;
	state[16] += state[9] ^ 0x1448b87d;
	extra_state &= state[19] ^ 0x7c5e8091;
	state[15] -= extra_state - 0x2de973cc;
	state[17] *= state[0] + 0x5cd4018;
	state[15] *= state[7] ^ 0x1c718ec4;
	ROREQ (state[4], ROL (state[14], 31));
	state[6] -= state[1] * 0x11e6e4aa;
	state[3] -= state[14] - 0x20a45ef;
	state[14] &= state[11] ^ 0x79362e5;
}

void mix_major21 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[2] ^ state[11] ^ state[15]) % 11;
	ROREQ (state[13], extra_state | 0x1e);
	extra_state -= state[6] - 0x67e07c3f;
	extra_state ^= extra_state * 0x157052aa;

	if (a == 1) {
		mix_minor22;
		mix_minor62;
		mix_minor63;
		mix_major6 (state, extra_state);
	}

	ROLEQ (state[6], ROR (state[6], 11));
	state[19] += extra_state * 0x2437b7c7;

	if (a == 6) {
		state[8] -= 0x383b7a6c;
		mix_minor28;
		state[14] -= 0xd8799ad3;
		mix_major4 (state, state[2]);
	}

	state[3] += state[12] + 0xf9430940;
	state[11] -= state[6];

	if (a == 5) {
		mix_minor40;
		mix_minor41;
		mix_minor66;
		mix_major11 (state, state[8]);
	}

	state[0] += state[14] | 0x27c78ea;
	state[18] -= extra_state & 0x6b2cc678;
	ROREQ (state[15], state[11] & 0xa);

	if (a == 4) {
		mix_minor67;
		mix_minor26;
		mix_minor21;
		mix_major22 (state, extra_state);
	}

	extra_state ^= ROR (state[18], 2);
	state[10] += state[15] * 0x42515298;
	state[19] += state[2] ^ 0x2a15668a;

	if (a == 7) {
		ROLEQ (state[19], 21);
		mix_minor30;
		mix_minor58;
		mix_major5 (state, state[11]);
	}

	state[6] -= extra_state - 0xe28d6e07;
	state[1] &= state[3] + 0x8a7848d;
	state[10] *= state[17] + 0xf76061aa;

	if (a == 0) {
		mix_minor29;
		state[17] ^= 0x3d87b641;
		mix_minor60;
		mix_major3 (state, state[12]);
	}

	state[6] += ROR (state[1], 8);
	state[1] *= state[2] | 0x16a41bdf;

	if (a == 8) {
		mix_minor68;
		state[9] += 0xb8c1b4ce;
		state[4] ^= 0x5c2840a0;
		mix_major14 (state, state[3]);
	}

	state[0] -= state[4] - 0x21889c31;
	extra_state *= extra_state ^ 0x14a9f943;
	state[5] |= state[13] + 0x5c58f04e;
	state[19] ^= state[14] + 0x49437c23;

	if (a == 2) {
		mix_minor22;
		mix_minor30;
		mix_minor29;
		mix_major9 (state, state[13]);
	}

	state[9] |= state[6] ^ 0x360a1ff0;
	state[13] &= state[14] * 0x810027b;
	extra_state += extra_state + 0x3053624;

	if (a == 3) {
		mix_minor20;
		mix_minor38;
		mix_minor27;
		mix_major10 (state, state[17]);
	}

	extra_state -= state[1] - 0xc7af02f5;
	extra_state &= 0xc11a9b11;
	state[6] ^= state[12] + 0xac2e6058;
	state[12] ^= state[17] + 0xd87e9f50;

	if (a == 10) {
		mix_minor37;
		mix_minor29;
		mix_minor31;
		mix_major2 (state, state[6]);
	}

	ROLEQ (state[9], state[7] ^ 0x1);
	extra_state += state[14] ^ 0xff63c7c;

	if (a == 9) {
		state[14] ^= 0x491ed97d;
		mix_minor22;
		mix_minor40;
		mix_major7 (state, state[5]);
	}

	state[9] ^= state[6] ^ 0x132ee304;
	state[12] *= state[14] + 0x11e0a175;
	state[14] -= extra_state ^ 0x267e2568;
	ROLEQ (state[0], ROL (state[3], 11));
	state[8] ^= state[6] ^ 0xe173238;
	state[0] *= state[6] + 0xee9e5b6a;
	state[9] |= state[15] * 0x1fe0f470;
	ROLEQ (state[2], state[2] + 0x9);
	state[16] ^= state[14] * 0x1b4bf87b;
	state[16] &= state[10] + 0x2383020a;
	state[15] += state[7] + 0xeb32d6f9;
	state[15] ^= ROL (state[16], 17);
	state[16] += extra_state | 0x20914367;
}

void mix_major22 (u32 *state, u32 extra_state)
{
	int a;

	a = extra_state % 11;
	state[12] += extra_state ^ 0xc3115e;
	state[19] -= extra_state - 0x4f9d3712;
	state[16] &= state[11] * 0x37e68d12;

	if (a == 1) {
		mix_minor57;
		state[4] *= 0x6f2b88b5;
		mix_major19 (state, state[7]);
	}

	extra_state -= state[18] ^ 0x4ea934da;
	state[1] &= state[18] ^ 0x18a1ba1a;

	if (a == 0) {
		state[3] += 0x9302840;
		state[8] += 0x91520abe;
		mix_minor37;
		mix_major20 (state, extra_state);
	}

	state[18] += state[17] * 0x3bf23dc7;
	state[12] += state[5] ^ 0x3537eae2;
	state[9] += state[5] + 0xf4d4e1ee;
	state[11] -= ROL (state[16], 22);

	if (a == 2) {
		mix_minor67;
		mix_minor67;
		mix_minor37;
		mix_major17 (state, state[16]);
	}

	ROLEQ (state[17], state[1]);
	extra_state |= ROR (state[5], 1);
	state[11] += extra_state + 0xf0871714;

	if (a == 3) {
		mix_minor35;
		mix_minor65;
		state[8] -= 0x265c0434;
		mix_major16 (state, state[18]);
	}

	extra_state &= 0x1b54f10;
	state[15] += state[1] + 0xe9b29695;
	state[9] ^= state[19] + 0xf9850900;

	if (a == 8) {
		ROLEQ (state[10], 1);
		state[4] *= 0xb27c0ecb;
		mix_minor61;
		mix_major15 (state, state[11]);
	}

	state[0] += state[6] + 0x224785;
	state[1] -= state[9] * 0x602a9ff;

	if (a == 4) {
		mix_minor53;
		state[3] += 0x9702000;
		mix_minor26;
		mix_major8 (state, state[10]);
	}

	state[14] += ROL (state[5], 30);
	state[8] -= state[12] * 0x223c8eff;
	state[3] += state[11] * 0xc99e9b5;

	if (a == 7) {
		mix_minor57;
		mix_minor45;
		mix_minor49;
		mix_major12 (state, state[13]);
	}

	extra_state *= state[3] ^ 0xf8e252d;
	state[12] += extra_state & 0xa58c765;

	if (a == 10) {
		mix_minor39;
		state[14] ^= 0x4dfb7ee4;
		mix_minor61;
		mix_major18 (state, state[13]);
	}

	state[11] -= state[3] ^ 0x59507436;
	state[10] ^= extra_state ^ 0x1082cbd7;

	if (a == 9) {
		mix_minor50;
		mix_minor53;
		ROLEQ (state[10], 6);
		mix_major6 (state, state[3]);
	}

	ROREQ (state[8], extra_state + 0x1);
	state[17] ^= state[15] * 0x1627a9f4;

	if (a == 6) {
		state[0] += 0xc3649533;
		mix_minor30;
		mix_minor41;
		mix_major4 (state, extra_state);
	}

	extra_state *= ROL (state[11], 21);
	state[3] ^= state[11] + 0x27d2e810;
	state[3] += state[16] * 0x2bb9259f;

	if (a == 5) {
		mix_minor40;
		mix_minor63;
		mix_minor61;
		mix_major11 (state, extra_state);
	}

	state[19] ^= state[17] ^ 0x2b7f6e80;
	state[7] ^= ROL (state[0], 24);
	state[4] ^= extra_state | 0x334e9536;
	state[11] -= ROL (extra_state, 19);
	state[12] ^= extra_state + 0xf8e5b64c;
	state[11] += state[4] + 0x661bc871;
	ROREQ (state[19], state[0] & 0x9);
	state[15] += extra_state & 0x7b85306;
	state[7] -= state[12] - 0x1394a239;
	state[17] ^= state[3] + 0x4d2d2d3c;
	state[12] -= state[6] & 0x312a10;
	state[13] |= state[19] + 0xba345c89;
	state[13] *= extra_state + 0x2098c7b4;
	state[6] &= ROL (state[10], 22);
	state[6] -= state[12] & 0x13175e3d;
}

void mix_major23 (u32 *state, u32 extra_state)
{
	int a;

	a = extra_state % 11;
	extra_state &= ROL (state[5], 11);
	state[18] -= ROR (extra_state, 23);
	state[19] += extra_state + 0xb42a2f00;

	if (a == 5) {
		state[17] ^= 0x33db0465;
		mix_minor62;
		mix_minor33;
		mix_major22 (state, state[2]);
	}

	state[0] += state[12] + 0x71507fd7;
	extra_state += state[19] + 0x9a68096;

	if (a == 6) {
		mix_minor64;
		mix_minor47;
		mix_minor62;
		mix_major5 (state, state[10]);
	}

	state[0] += state[2] + 0x238788d8;
	ROREQ (state[3], state[15] + 0x16);
	state[10] -= state[9] - 0xdf1e2fab;

	if (a == 3) {
		mix_minor50;
		state[4] ^= 0x3f348b71;
		mix_minor44;
		mix_major3 (state, extra_state);
	}

	extra_state ^= ROL (extra_state, 27);
	extra_state -= ROR (state[11], 23);

	if (a == 7) {
		mix_minor48;
		mix_minor51;
		state[3] -= 0xf2da1eb7;
		mix_major14 (state, state[15]);
	}

	state[18] += state[10] + 0x13ba6066;
	state[11] -= state[10] - 0xd44a337d;
	state[17] &= state[3] + 0xad722336;

	if (a == 4) {
		state[3] *= 0x1e952879;
		mix_minor55;
		mix_minor52;
		mix_major9 (state, state[8]);
	}

	state[7] -= ROL (extra_state, 15);
	extra_state |= state[6] + 0x45d2e311;
	extra_state ^= state[7] + 0xd196f18f;
	ROLEQ (state[7], extra_state ^ 0x8);

	if (a == 8) {
		mix_minor55;
		mix_minor64;
		state[3] += 0x704000;
		mix_major10 (state, state[13]);
	}

	state[6] += state[18] * 0x413db8c1;
	state[0] ^= state[19] + 0x2be41642;
	state[4] *= ROR (state[9], 18);

	if (a == 10) {
		mix_minor26;
		mix_minor47;
		mix_minor59;
		mix_major2 (state, extra_state);
	}

	ROLEQ (extra_state, state[6] * 0x13);
	state[17] *= state[3] & 0x9262077;
	state[13] ^= state[14] + 0xfa8ae5a0;

	if (a == 1) {
		mix_minor65;
		mix_minor21;
		mix_minor24;
		mix_major7 (state, state[13]);
	}

	ROREQ (extra_state, ROL (state[2], 17));
	state[13] -= state[8] - 0xffd58fe8;
	state[8] += state[6] ^ 0x1d606322;

	if (a == 9) {
		mix_minor52;
		mix_minor26;
		state[3] += 0x404840;
		mix_major21 (state, state[10]);
	}

	state[16] += state[19] + 0xe3a240f7;
	extra_state ^= ROR (state[14], 3);

	if (a == 0) {
		mix_minor43;
		state[3] -= 0xa9fe8c6d;
		state[0] += 0xe9a284bb;
		mix_major13 (state, state[4]);
	}

	state[18] ^= state[7] | 0x196e1a4c;
	extra_state += state[18] ^ 0xffcac8f;
	state[1] ^= state[0] ^ 0xb09adec;

	if (a == 2) {
		mix_minor53;
		mix_minor39;
		mix_minor67;
		mix_major24 (state, state[11]);
	}

	state[14] *= state[2] + 0x328852b1;
	state[8] ^= state[15] & 0x1e0a37a;
	state[3] *= ROL (extra_state, 13);
	state[6] ^= state[18] + 0xc9c48b38;
	ROLEQ (state[2], state[14] + 0x1d);
	extra_state ^= ROR (state[10], 13);
	state[12] ^= state[8] + 0xef774f5b;
	ROREQ (extra_state, state[14] + 0x3);
	extra_state += extra_state ^ 0x58f00a07;
	state[9] ^= extra_state ^ 0x5483deb2;
	state[14] |= state[0] * 0x2c63f116;
	state[3] += state[10] ^ 0xa051af;
	extra_state -= state[0] - 0xfdb247f0;
	state[2] -= extra_state - 0xf9432db1;
}

void mix_major24 (u32 *state, u32 extra_state)
{
	int a;

	a = (state[17] ^ state[8] ^ state[10]) % 11;
	extra_state *= state[7];
	extra_state ^= state[0] ^ 0x13a77c41;
	ROLEQ (state[2], state[3] + 0x10);

	if (a == 1) {
		mix_minor27;
		mix_minor59;
		mix_minor22;
		mix_major11 (state, state[12]);
	}

	extra_state ^= extra_state + 0xf4135aef;
	ROLEQ (extra_state, state[6] + 0x9);
	state[14] += ROL (state[13], 25);
	state[16] ^= state[8] + 0x19454e81;

	if (a == 10) {
		mix_minor63;
		mix_minor32;
		mix_minor29;
		mix_major22 (state, state[8]);
	}

	state[3] *= extra_state + 0xcb4ea17e;
	ROLEQ (state[17], state[17] ^ 0x14);
	extra_state -= state[11] * 0x2c0fd99b;

	if (a == 3) {
		mix_minor57;
		mix_minor63;
		mix_minor25;
		mix_major5 (state, extra_state);
	}

	state[12] += state[19] + 0x7e55995;
	state[14] -= state[13] * 0x3dd1a491;
	state[4] |= state[8] & 0x162b97ec;
	state[8] += state[3] + 0xc3000fb6;

	if (a == 6) {
		mix_minor48;
		state[8] += 0x9cd4867c;
		state[14] += 0x79cdbac7;
		mix_major3 (state, state[9]);
	}

	state[13] += state[8] ^ 0x2a161224;
	state[10] += state[1] * 0xc693c6b;
	state[4] *= state[10] + 0xecde6b96;

	if (a == 9) {
		mix_minor40;
		ROLEQ (state[10], 18);
		mix_minor40;
		mix_major14 (state, state[13]);
	}

	state[8] *= ROR (state[13], 25);
	state[17] ^= ROR (state[14], 24);
	extra_state &= state[4] + 0x1c938114;

	if (a == 2) {
		state[4] ^= 0xc25fdd85;
		mix_minor42;
		mix_minor27;
		mix_major9 (state, extra_state);
	}

	state[0] *= extra_state + 0xc328858;
	extra_state += state[15] | 0x137d6d8;
	state[3] -= state[9] - 0xae4f0ae;

	if (a == 0) {
		mix_minor45;
		mix_minor24;
		mix_minor51;
		mix_major10 (state, state[3]);
	}

	extra_state *= state[10] + 0xe55615;
	state[15] |= extra_state | 0x120d32e3;
	ROLEQ (extra_state, state[15] ^ 0xc);
	ROREQ (state[6], state[7]);

	if (a == 7) {
		mix_minor23;
		mix_minor57;
		mix_minor55;
		mix_major2 (state, state[17]);
	}

	state[3] -= state[4] | 0x2587388f;
	state[2] += state[4] + 0xffda87c9;
	extra_state -= ROR (state[2], 17);
	state[1] += state[6] * 0x34aabe3a;

	if (a == 4) {
		mix_minor30;
		mix_minor46;
		mix_minor57;
		mix_major7 (state, state[16]);
	}

	state[17] ^= state[13] ^ 0x3d17e55a;
	state[15] *= state[14] + 0xdaf5121;

	if (a == 5) {
		mix_minor57;
		mix_minor60;
		mix_minor61;
		mix_major21 (state, extra_state);
	}

	ROLEQ (state[6], state[17] * 0x14);
	state[6] += state[15] ^ 0x14819516;

	if (a == 8) {
		state[8] -= 0x7b22975e;
		mix_minor50;
		mix_minor56;
		mix_major13 (state, state[5]);
	}

	state[8] |= state[14] + 0xc735f228;
	ROREQ (state[7], state[17] + 0x1e);
	extra_state *= state[10] * 0x340d3ff2;
	state[16] *= state[14] + 0x57a8d4b3;
	state[6] -= state[1] - 0x534be48e;
	state[2] ^= state[9] * 0xd695251;
	state[12] ^= state[7];
	state[1] += state[17] + 0xf022cb99;
	state[4] += extra_state | 0x2954ac20;
	state[7] *= extra_state ^ 0x1b904466;
	state[2] -= extra_state * 0x31fef0e1;
}
