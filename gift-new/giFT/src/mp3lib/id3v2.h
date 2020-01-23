//  mp3info - reads mp3 headers/tags information
//  Copyright (C) 2002  Shachar Raindel
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this library; if not, write to the Free
//  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
//  MA 02111-1307, USA
//
#ifndef MP3PLS_ID3V2_H
#define MP3PLS_ID3V2_H

/*-----------------10/12/01 17:32-------------------
 * for any ID3v2 tag
 * --------------------------------------------------*/
#define ID3V2_MAIN_HEADER_SIZE 10

extern const char *ID3V2_HEADER_MAGIC;  /* from mp3list.cpp */

/*-----------------10/12/01 17:32-------------------
 * For ID3v2.2
 * --------------------------------------------------*/

#define ID3v2_DOT_2_FRAME_HEADER_SIZE 6
#define MAKE_INT_FROM_3_CHARS( a , b , c ) (((a)<<16) | ((b)<<8) | (c) )

typedef enum id3v2_dot_2_frames_types_enum_tag
{
  ID3v2_DOT_2_FRAME_ID_BUF = MAKE_INT_FROM_3_CHARS( 'B' , 'U' , 'F' ),
  ID3v2_DOT_2_FRAME_ID_CNT = MAKE_INT_FROM_3_CHARS( 'C' , 'N' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_COM = MAKE_INT_FROM_3_CHARS( 'C' , 'O' , 'M' ),
  ID3v2_DOT_2_FRAME_ID_CRA = MAKE_INT_FROM_3_CHARS( 'C' , 'R' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_CRM = MAKE_INT_FROM_3_CHARS( 'C' , 'R' , 'M' ),
  ID3v2_DOT_2_FRAME_ID_ETC = MAKE_INT_FROM_3_CHARS( 'E' , 'T' , 'C' ),
  ID3v2_DOT_2_FRAME_ID_EQU = MAKE_INT_FROM_3_CHARS( 'E' , 'Q' , 'U' ),
  ID3v2_DOT_2_FRAME_ID_GEO = MAKE_INT_FROM_3_CHARS( 'G' , 'E' , 'O' ),
  ID3v2_DOT_2_FRAME_ID_IPL = MAKE_INT_FROM_3_CHARS( 'I' , 'P' , 'L' ),
  ID3v2_DOT_2_FRAME_ID_LNK = MAKE_INT_FROM_3_CHARS( 'L' , 'N' , 'K' ),
  ID3v2_DOT_2_FRAME_ID_MCI = MAKE_INT_FROM_3_CHARS( 'M' , 'C' , 'I' ),
  ID3v2_DOT_2_FRAME_ID_MLL = MAKE_INT_FROM_3_CHARS( 'M' , 'L' , 'L' ),
  ID3v2_DOT_2_FRAME_ID_PIC = MAKE_INT_FROM_3_CHARS( 'P' , 'I' , 'C' ),
  ID3v2_DOT_2_FRAME_ID_POP = MAKE_INT_FROM_3_CHARS( 'P' , 'O' , 'P' ),
  ID3v2_DOT_2_FRAME_ID_REV = MAKE_INT_FROM_3_CHARS( 'R' , 'E' , 'V' ),
  ID3v2_DOT_2_FRAME_ID_RVA = MAKE_INT_FROM_3_CHARS( 'R' , 'V' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_SLT = MAKE_INT_FROM_3_CHARS( 'S' , 'L' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_STC = MAKE_INT_FROM_3_CHARS( 'S' , 'T' , 'C' ),
  ID3v2_DOT_2_FRAME_ID_TAL = MAKE_INT_FROM_3_CHARS( 'T' , 'A' , 'L' ),
  ID3v2_DOT_2_FRAME_ID_TBP = MAKE_INT_FROM_3_CHARS( 'T' , 'B' , 'P' ),
  ID3v2_DOT_2_FRAME_ID_TCM = MAKE_INT_FROM_3_CHARS( 'T' , 'C' , 'M' ),
  ID3v2_DOT_2_FRAME_ID_TCO = MAKE_INT_FROM_3_CHARS( 'T' , 'C' , 'O' ),
  ID3v2_DOT_2_FRAME_ID_TCR = MAKE_INT_FROM_3_CHARS( 'T' , 'C' , 'R' ),
  ID3v2_DOT_2_FRAME_ID_TDA = MAKE_INT_FROM_3_CHARS( 'T' , 'D' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_TDY = MAKE_INT_FROM_3_CHARS( 'T' , 'D' , 'Y' ),
  ID3v2_DOT_2_FRAME_ID_TEN = MAKE_INT_FROM_3_CHARS( 'T' , 'E' , 'N' ),
  ID3v2_DOT_2_FRAME_ID_TFT = MAKE_INT_FROM_3_CHARS( 'T' , 'F' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_TIM = MAKE_INT_FROM_3_CHARS( 'T' , 'I' , 'M' ),
  ID3v2_DOT_2_FRAME_ID_TKE = MAKE_INT_FROM_3_CHARS( 'T' , 'K' , 'E' ),
  ID3v2_DOT_2_FRAME_ID_TLA = MAKE_INT_FROM_3_CHARS( 'T' , 'L' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_TLE = MAKE_INT_FROM_3_CHARS( 'T' , 'L' , 'E' ),
  ID3v2_DOT_2_FRAME_ID_TMT = MAKE_INT_FROM_3_CHARS( 'T' , 'M' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_TOA = MAKE_INT_FROM_3_CHARS( 'T' , 'O' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_TOF = MAKE_INT_FROM_3_CHARS( 'T' , 'O' , 'F' ),
  ID3v2_DOT_2_FRAME_ID_TOL = MAKE_INT_FROM_3_CHARS( 'T' , 'O' , 'L' ),
  ID3v2_DOT_2_FRAME_ID_TOR = MAKE_INT_FROM_3_CHARS( 'T' , 'O' , 'R' ),
  ID3v2_DOT_2_FRAME_ID_TOT = MAKE_INT_FROM_3_CHARS( 'T' , 'O' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_TP1 = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , '1' ),
  ID3v2_DOT_2_FRAME_ID_TP2 = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , '2' ),
  ID3v2_DOT_2_FRAME_ID_TP3 = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , '3' ),
  ID3v2_DOT_2_FRAME_ID_TP4 = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , '4' ),
  ID3v2_DOT_2_FRAME_ID_TPA = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , 'A' ),
  ID3v2_DOT_2_FRAME_ID_TPB = MAKE_INT_FROM_3_CHARS( 'T' , 'P' , 'B' ),
  ID3v2_DOT_2_FRAME_ID_TRC = MAKE_INT_FROM_3_CHARS( 'T' , 'R' , 'C' ),
  ID3v2_DOT_2_FRAME_ID_TRD = MAKE_INT_FROM_3_CHARS( 'T' , 'R' , 'D' ),
  ID3v2_DOT_2_FRAME_ID_TRK = MAKE_INT_FROM_3_CHARS( 'T' , 'R' , 'K' ),
  ID3v2_DOT_2_FRAME_ID_TSI = MAKE_INT_FROM_3_CHARS( 'T' , 'S' , 'I' ),
  ID3v2_DOT_2_FRAME_ID_TSS = MAKE_INT_FROM_3_CHARS( 'T' , 'S' , 'S' ),
  ID3v2_DOT_2_FRAME_ID_TT1 = MAKE_INT_FROM_3_CHARS( 'T' , 'T' , '1' ),
  ID3v2_DOT_2_FRAME_ID_TT2 = MAKE_INT_FROM_3_CHARS( 'T' , 'T' , '2' ),
  ID3v2_DOT_2_FRAME_ID_TT3 = MAKE_INT_FROM_3_CHARS( 'T' , 'T' , '3' ),
  ID3v2_DOT_2_FRAME_ID_TXT = MAKE_INT_FROM_3_CHARS( 'T' , 'X' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_TXX = MAKE_INT_FROM_3_CHARS( 'T' , 'X' , 'X' ),
  ID3v2_DOT_2_FRAME_ID_TYE = MAKE_INT_FROM_3_CHARS( 'T' , 'Y' , 'E' ),
  ID3v2_DOT_2_FRAME_ID_UFI = MAKE_INT_FROM_3_CHARS( 'U' , 'F' , 'I' ),
  ID3v2_DOT_2_FRAME_ID_ULT = MAKE_INT_FROM_3_CHARS( 'U' , 'L' , 'T' ),
  ID3v2_DOT_2_FRAME_ID_WAF = MAKE_INT_FROM_3_CHARS( 'W' , 'A' , 'F' ),
  ID3v2_DOT_2_FRAME_ID_WAR = MAKE_INT_FROM_3_CHARS( 'W' , 'A' , 'R' ),
  ID3v2_DOT_2_FRAME_ID_WAS = MAKE_INT_FROM_3_CHARS( 'W' , 'A' , 'S' ),
  ID3v2_DOT_2_FRAME_ID_WCM = MAKE_INT_FROM_3_CHARS( 'W' , 'C' , 'M' ),
  ID3v2_DOT_2_FRAME_ID_WCP = MAKE_INT_FROM_3_CHARS( 'W' , 'C' , 'P' ),
  ID3v2_DOT_2_FRAME_ID_WPB = MAKE_INT_FROM_3_CHARS( 'W' , 'P' , 'B' ),
  ID3v2_DOT_2_FRAME_ID_WXX = MAKE_INT_FROM_3_CHARS( 'W' , 'X' , 'X' ),
  ID3v2_DOT_2_FRAME_ID_CDM = MAKE_INT_FROM_3_CHARS( 'C' , 'D' , 'M' ),
} id3v2_dot_2_frames_types_enum;
const id3v2_dot_2_frames_types_enum known_id3v2_dot_2_frames[] = {
  ID3v2_DOT_2_FRAME_ID_BUF,             /* 01 */
  ID3v2_DOT_2_FRAME_ID_CNT,             /* 02 */
  ID3v2_DOT_2_FRAME_ID_COM,             /* 03 */
  ID3v2_DOT_2_FRAME_ID_CRA,             /* 04 */
  ID3v2_DOT_2_FRAME_ID_CRM,             /* 05 */
  ID3v2_DOT_2_FRAME_ID_ETC,             /* 06 */
  ID3v2_DOT_2_FRAME_ID_EQU,             /* 07 */
  ID3v2_DOT_2_FRAME_ID_GEO,             /* 08 */
  ID3v2_DOT_2_FRAME_ID_IPL,             /* 09 */
  ID3v2_DOT_2_FRAME_ID_LNK,             /* 10 */
  ID3v2_DOT_2_FRAME_ID_MCI,             /* 11 */
  ID3v2_DOT_2_FRAME_ID_MLL,             /* 12 */
  ID3v2_DOT_2_FRAME_ID_PIC,             /* 13 */
  ID3v2_DOT_2_FRAME_ID_POP,             /* 14 */
  ID3v2_DOT_2_FRAME_ID_REV,             /* 15 */
  ID3v2_DOT_2_FRAME_ID_RVA,             /* 16 */
  ID3v2_DOT_2_FRAME_ID_SLT,             /* 17 */
  ID3v2_DOT_2_FRAME_ID_STC,             /* 18 */
  ID3v2_DOT_2_FRAME_ID_TAL,             /* 19 */
  ID3v2_DOT_2_FRAME_ID_TBP,             /* 20 */
  ID3v2_DOT_2_FRAME_ID_TCM,             /* 21 */
  ID3v2_DOT_2_FRAME_ID_TCO,             /* 22 */
  ID3v2_DOT_2_FRAME_ID_TCR,             /* 23 */
  ID3v2_DOT_2_FRAME_ID_TDA,             /* 24 */
  ID3v2_DOT_2_FRAME_ID_TDY,             /* 25 */
  ID3v2_DOT_2_FRAME_ID_TEN,             /* 26 */
  ID3v2_DOT_2_FRAME_ID_TFT,             /* 27 */
  ID3v2_DOT_2_FRAME_ID_TIM,             /* 28 */
  ID3v2_DOT_2_FRAME_ID_TKE,             /* 29 */
  ID3v2_DOT_2_FRAME_ID_TLA,             /* 30 */
  ID3v2_DOT_2_FRAME_ID_TLE,             /* 31 */
  ID3v2_DOT_2_FRAME_ID_TMT,             /* 32 */
  ID3v2_DOT_2_FRAME_ID_TOA,             /* 33 */
  ID3v2_DOT_2_FRAME_ID_TOF,             /* 34 */
  ID3v2_DOT_2_FRAME_ID_TOL,             /* 35 */
  ID3v2_DOT_2_FRAME_ID_TOR,             /* 36 */
  ID3v2_DOT_2_FRAME_ID_TOT,             /* 37 */
  ID3v2_DOT_2_FRAME_ID_TP1,             /* 38 */
  ID3v2_DOT_2_FRAME_ID_TP2,             /* 39 */
  ID3v2_DOT_2_FRAME_ID_TP3,             /* 40 */
  ID3v2_DOT_2_FRAME_ID_TP4,             /* 41 */
  ID3v2_DOT_2_FRAME_ID_TPA,             /* 42 */
  ID3v2_DOT_2_FRAME_ID_TPB,             /* 43 */
  ID3v2_DOT_2_FRAME_ID_TRC,             /* 44 */
  ID3v2_DOT_2_FRAME_ID_TRD,             /* 45 */
  ID3v2_DOT_2_FRAME_ID_TRK,             /* 46 */
  ID3v2_DOT_2_FRAME_ID_TSI,             /* 47 */
  ID3v2_DOT_2_FRAME_ID_TSS,             /* 48 */
  ID3v2_DOT_2_FRAME_ID_TT1,             /* 49 */
  ID3v2_DOT_2_FRAME_ID_TT2,             /* 50 */
  ID3v2_DOT_2_FRAME_ID_TT3,             /* 51 */
  ID3v2_DOT_2_FRAME_ID_TXT,             /* 52 */
  ID3v2_DOT_2_FRAME_ID_TXX,             /* 53 */
  ID3v2_DOT_2_FRAME_ID_TYE,             /* 54 */
  ID3v2_DOT_2_FRAME_ID_UFI,             /* 55 */
  ID3v2_DOT_2_FRAME_ID_ULT,             /* 56 */
  ID3v2_DOT_2_FRAME_ID_WAF,             /* 57 */
  ID3v2_DOT_2_FRAME_ID_WAR,             /* 58 */
  ID3v2_DOT_2_FRAME_ID_WAS,             /* 59 */
  ID3v2_DOT_2_FRAME_ID_WCM,             /* 60 */
  ID3v2_DOT_2_FRAME_ID_WCP,             /* 61 */
  ID3v2_DOT_2_FRAME_ID_WPB,             /* 62 */
  ID3v2_DOT_2_FRAME_ID_WXX,             /* 63 */
  ID3v2_DOT_2_FRAME_ID_CDM,             /* 64 */
};

#define ID3v2_DOT_2_KNOWN_FRAMES_NUMBER 64 /*((sizeof(known_id3v2_dot_2_frames))/(sizeof(id3v2_dot_2_frames_types_enum)))*/


/*-----------------10/12/01 17:32-------------------
 * For ID3v2.3
 * --------------------------------------------------*/

#define ID3v2_DOT_3_FRAME_HEADER_SIZE 10
#define MAKE_INT_FROM_4_CHARS( a , b , c , d ) (((a)<<24) | ((b)<<16) | ((c)<<8) | (d) )


typedef enum id3v2_dot_3_frames_types_enum_tag
{
  ID3v2_DOT_3_FRAME_ID_AENC = MAKE_INT_FROM_4_CHARS( 'A' , 'E' , 'N' , 'C' ),
  ID3v2_DOT_3_FRAME_ID_APIC = MAKE_INT_FROM_4_CHARS( 'A' , 'P' , 'I' , 'C' ),
  ID3v2_DOT_3_FRAME_ID_COMM = MAKE_INT_FROM_4_CHARS( 'C' , 'O' , 'M' , 'M' ),
  ID3v2_DOT_3_FRAME_ID_COMR = MAKE_INT_FROM_4_CHARS( 'C' , 'O' , 'M' , 'R' ),
  ID3v2_DOT_3_FRAME_ID_ENCR = MAKE_INT_FROM_4_CHARS( 'E' , 'N' , 'C' , 'R' ),
  ID3v2_DOT_3_FRAME_ID_EQUA = MAKE_INT_FROM_4_CHARS( 'E' , 'Q' , 'U' , 'A' ),
  ID3v2_DOT_3_FRAME_ID_ETCO = MAKE_INT_FROM_4_CHARS( 'E' , 'T' , 'C' , 'O' ),
  ID3v2_DOT_3_FRAME_ID_GEOB = MAKE_INT_FROM_4_CHARS( 'G' , 'E' , 'O' , 'B' ),
  ID3v2_DOT_3_FRAME_ID_GRID = MAKE_INT_FROM_4_CHARS( 'G' , 'R' , 'I' , 'D' ),
  ID3v2_DOT_3_FRAME_ID_IPLS = MAKE_INT_FROM_4_CHARS( 'I' , 'P' , 'L' , 'S' ),
  ID3v2_DOT_3_FRAME_ID_LINK = MAKE_INT_FROM_4_CHARS( 'L' , 'I' , 'N' , 'K' ),
  ID3v2_DOT_3_FRAME_ID_MCDI = MAKE_INT_FROM_4_CHARS( 'M' , 'C' , 'D' , 'I' ),
  ID3v2_DOT_3_FRAME_ID_MLLT = MAKE_INT_FROM_4_CHARS( 'M' , 'L' , 'L' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_OWNE = MAKE_INT_FROM_4_CHARS( 'O' , 'W' , 'N' , 'E' ),
  ID3v2_DOT_3_FRAME_ID_PRIV = MAKE_INT_FROM_4_CHARS( 'P' , 'R' , 'I' , 'V' ),
  ID3v2_DOT_3_FRAME_ID_PCNT = MAKE_INT_FROM_4_CHARS( 'P' , 'C' , 'N' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_POPM = MAKE_INT_FROM_4_CHARS( 'P' , 'O' , 'P' , 'M' ),
  ID3v2_DOT_3_FRAME_ID_POSS = MAKE_INT_FROM_4_CHARS( 'P' , 'O' , 'S' , 'S' ),
  ID3v2_DOT_3_FRAME_ID_RBUF = MAKE_INT_FROM_4_CHARS( 'R' , 'B' , 'U' , 'F' ),
  ID3v2_DOT_3_FRAME_ID_RVAD = MAKE_INT_FROM_4_CHARS( 'R' , 'V' , 'A' , 'D' ),
  ID3v2_DOT_3_FRAME_ID_RVRB = MAKE_INT_FROM_4_CHARS( 'R' , 'V' , 'R' , 'B' ),
  ID3v2_DOT_3_FRAME_ID_SYLT = MAKE_INT_FROM_4_CHARS( 'S' , 'Y' , 'L' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_SYTC = MAKE_INT_FROM_4_CHARS( 'S' , 'Y' , 'T' , 'C' ),
  ID3v2_DOT_3_FRAME_ID_TALB = MAKE_INT_FROM_4_CHARS( 'T' , 'A' , 'L' , 'B' ),
  ID3v2_DOT_3_FRAME_ID_TBPM = MAKE_INT_FROM_4_CHARS( 'T' , 'B' , 'P' , 'M' ),
  ID3v2_DOT_3_FRAME_ID_TCOM = MAKE_INT_FROM_4_CHARS( 'T' , 'C' , 'O' , 'M' ),
  ID3v2_DOT_3_FRAME_ID_TCON = MAKE_INT_FROM_4_CHARS( 'T' , 'C' , 'O' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TCOP = MAKE_INT_FROM_4_CHARS( 'T' , 'C' , 'O' , 'P' ),
  ID3v2_DOT_3_FRAME_ID_TDAT = MAKE_INT_FROM_4_CHARS( 'T' , 'D' , 'A' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_TDLY = MAKE_INT_FROM_4_CHARS( 'T' , 'D' , 'L' , 'Y' ),
  ID3v2_DOT_3_FRAME_ID_TENC = MAKE_INT_FROM_4_CHARS( 'T' , 'E' , 'N' , 'C' ),
  ID3v2_DOT_3_FRAME_ID_TEXT = MAKE_INT_FROM_4_CHARS( 'T' , 'E' , 'X' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_TFLT = MAKE_INT_FROM_4_CHARS( 'T' , 'F' , 'L' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_TIME = MAKE_INT_FROM_4_CHARS( 'T' , 'I' , 'M' , 'E' ),
  ID3v2_DOT_3_FRAME_ID_TIT1 = MAKE_INT_FROM_4_CHARS( 'T' , 'I' , 'T' , '1' ),
  ID3v2_DOT_3_FRAME_ID_TIT2 = MAKE_INT_FROM_4_CHARS( 'T' , 'I' , 'T' , '2' ),
  ID3v2_DOT_3_FRAME_ID_TIT3 = MAKE_INT_FROM_4_CHARS( 'T' , 'I' , 'T' , '3' ),
  ID3v2_DOT_3_FRAME_ID_TKEY = MAKE_INT_FROM_4_CHARS( 'T' , 'K' , 'E' , 'Y' ),
  ID3v2_DOT_3_FRAME_ID_TLAN = MAKE_INT_FROM_4_CHARS( 'T' , 'L' , 'A' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TLEN = MAKE_INT_FROM_4_CHARS( 'T' , 'L' , 'E' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TMED = MAKE_INT_FROM_4_CHARS( 'T' , 'M' , 'E' , 'D' ),
  ID3v2_DOT_3_FRAME_ID_TOAL = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'A' , 'L' ),
  ID3v2_DOT_3_FRAME_ID_TOFN = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'F' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TOLY = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'L' , 'Y' ),
  ID3v2_DOT_3_FRAME_ID_TOPE = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'P' , 'E' ),
  ID3v2_DOT_3_FRAME_ID_TORY = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'R' , 'Y' ),
  ID3v2_DOT_3_FRAME_ID_TOWN = MAKE_INT_FROM_4_CHARS( 'T' , 'O' , 'W' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TPE1 = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'E' , '1' ),
  ID3v2_DOT_3_FRAME_ID_TPE2 = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'E' , '2' ),
  ID3v2_DOT_3_FRAME_ID_TPE3 = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'E' , '3' ),
  ID3v2_DOT_3_FRAME_ID_TPE4 = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'E' , '4' ),
  ID3v2_DOT_3_FRAME_ID_TPOS = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'O' , 'S' ),
  ID3v2_DOT_3_FRAME_ID_TPUB = MAKE_INT_FROM_4_CHARS( 'T' , 'P' , 'U' , 'B' ),
  ID3v2_DOT_3_FRAME_ID_TRCK = MAKE_INT_FROM_4_CHARS( 'T' , 'R' , 'C' , 'K' ),
  ID3v2_DOT_3_FRAME_ID_TRDA = MAKE_INT_FROM_4_CHARS( 'T' , 'R' , 'D' , 'A' ),
  ID3v2_DOT_3_FRAME_ID_TRSN = MAKE_INT_FROM_4_CHARS( 'T' , 'R' , 'S' , 'N' ),
  ID3v2_DOT_3_FRAME_ID_TRSO = MAKE_INT_FROM_4_CHARS( 'T' , 'R' , 'S' , 'O' ),
  ID3v2_DOT_3_FRAME_ID_TSIZ = MAKE_INT_FROM_4_CHARS( 'T' , 'S' , 'I' , 'Z' ),
  ID3v2_DOT_3_FRAME_ID_TSRC = MAKE_INT_FROM_4_CHARS( 'T' , 'S' , 'R' , 'C' ),
  ID3v2_DOT_3_FRAME_ID_TSSE = MAKE_INT_FROM_4_CHARS( 'T' , 'S' , 'S' , 'E' ),
  ID3v2_DOT_3_FRAME_ID_TYER = MAKE_INT_FROM_4_CHARS( 'T' , 'Y' , 'E' , 'R' ),
  ID3v2_DOT_3_FRAME_ID_TXXX = MAKE_INT_FROM_4_CHARS( 'T' , 'X' , 'X' , 'X' ),
  ID3v2_DOT_3_FRAME_ID_UFID = MAKE_INT_FROM_4_CHARS( 'U' , 'F' , 'I' , 'D' ),
  ID3v2_DOT_3_FRAME_ID_USER = MAKE_INT_FROM_4_CHARS( 'U' , 'S' , 'E' , 'R' ),
  ID3v2_DOT_3_FRAME_ID_USLT = MAKE_INT_FROM_4_CHARS( 'U' , 'S' , 'L' , 'T' ),
  ID3v2_DOT_3_FRAME_ID_WCOM = MAKE_INT_FROM_4_CHARS( 'W' , 'C' , 'O' , 'M' ),
  ID3v2_DOT_3_FRAME_ID_WCOP = MAKE_INT_FROM_4_CHARS( 'W' , 'C' , 'O' , 'P' ),
  ID3v2_DOT_3_FRAME_ID_WOAF = MAKE_INT_FROM_4_CHARS( 'W' , 'O' , 'A' , 'F' ),
  ID3v2_DOT_3_FRAME_ID_WOAR = MAKE_INT_FROM_4_CHARS( 'W' , 'O' , 'A' , 'R' ),
  ID3v2_DOT_3_FRAME_ID_WOAS = MAKE_INT_FROM_4_CHARS( 'W' , 'O' , 'A' , 'S' ),
  ID3v2_DOT_3_FRAME_ID_WORS = MAKE_INT_FROM_4_CHARS( 'W' , 'O' , 'R' , 'S' ),
  ID3v2_DOT_3_FRAME_ID_WPAY = MAKE_INT_FROM_4_CHARS( 'W' , 'P' , 'A' , 'Y' ),
  ID3v2_DOT_3_FRAME_ID_WPUB = MAKE_INT_FROM_4_CHARS( 'W' , 'P' , 'U' , 'B' ),
  ID3v2_DOT_3_FRAME_ID_WXXX = MAKE_INT_FROM_4_CHARS( 'W' , 'X' , 'X' , 'X' ),
} id3v2_dot_3_frames_types_enum;
const id3v2_dot_3_frames_types_enum known_id3v2_dot_3_frames[] = {
  ID3v2_DOT_3_FRAME_ID_AENC,             /* 01 */
  ID3v2_DOT_3_FRAME_ID_APIC,             /* 02 */
  ID3v2_DOT_3_FRAME_ID_COMM,             /* 03 */
  ID3v2_DOT_3_FRAME_ID_COMR,             /* 04 */
  ID3v2_DOT_3_FRAME_ID_ENCR,             /* 05 */
  ID3v2_DOT_3_FRAME_ID_EQUA,             /* 06 */
  ID3v2_DOT_3_FRAME_ID_ETCO,             /* 07 */
  ID3v2_DOT_3_FRAME_ID_GEOB,             /* 08 */
  ID3v2_DOT_3_FRAME_ID_GRID,             /* 09 */
  ID3v2_DOT_3_FRAME_ID_IPLS,             /* 10 */
  ID3v2_DOT_3_FRAME_ID_LINK,             /* 11 */
  ID3v2_DOT_3_FRAME_ID_MCDI,             /* 12 */
  ID3v2_DOT_3_FRAME_ID_MLLT,             /* 13 */
  ID3v2_DOT_3_FRAME_ID_OWNE,             /* 14 */
  ID3v2_DOT_3_FRAME_ID_PRIV,             /* 15 */
  ID3v2_DOT_3_FRAME_ID_PCNT,             /* 16 */
  ID3v2_DOT_3_FRAME_ID_POPM,             /* 17 */
  ID3v2_DOT_3_FRAME_ID_POSS,             /* 18 */
  ID3v2_DOT_3_FRAME_ID_RBUF,             /* 19 */
  ID3v2_DOT_3_FRAME_ID_RVAD,             /* 20 */
  ID3v2_DOT_3_FRAME_ID_RVRB,             /* 21 */
  ID3v2_DOT_3_FRAME_ID_SYLT,             /* 22 */
  ID3v2_DOT_3_FRAME_ID_SYTC,             /* 23 */
  ID3v2_DOT_3_FRAME_ID_TALB,             /* 24 */
  ID3v2_DOT_3_FRAME_ID_TBPM,             /* 25 */
  ID3v2_DOT_3_FRAME_ID_TCOM,             /* 26 */
  ID3v2_DOT_3_FRAME_ID_TCON,             /* 27 */
  ID3v2_DOT_3_FRAME_ID_TCOP,             /* 28 */
  ID3v2_DOT_3_FRAME_ID_TDAT,             /* 29 */
  ID3v2_DOT_3_FRAME_ID_TDLY,             /* 30 */
  ID3v2_DOT_3_FRAME_ID_TENC,             /* 31 */
  ID3v2_DOT_3_FRAME_ID_TEXT,             /* 32 */
  ID3v2_DOT_3_FRAME_ID_TFLT,             /* 33 */
  ID3v2_DOT_3_FRAME_ID_TIME,             /* 34 */
  ID3v2_DOT_3_FRAME_ID_TIT1,             /* 35 */
  ID3v2_DOT_3_FRAME_ID_TIT2,             /* 36 */
  ID3v2_DOT_3_FRAME_ID_TIT3,             /* 37 */
  ID3v2_DOT_3_FRAME_ID_TKEY,             /* 38 */
  ID3v2_DOT_3_FRAME_ID_TLAN,             /* 39 */
  ID3v2_DOT_3_FRAME_ID_TLEN,             /* 40 */
  ID3v2_DOT_3_FRAME_ID_TMED,             /* 41 */
  ID3v2_DOT_3_FRAME_ID_TOAL,             /* 42 */
  ID3v2_DOT_3_FRAME_ID_TOFN,             /* 43 */
  ID3v2_DOT_3_FRAME_ID_TOLY,             /* 44 */
  ID3v2_DOT_3_FRAME_ID_TOPE,             /* 45 */
  ID3v2_DOT_3_FRAME_ID_TORY,             /* 46 */
  ID3v2_DOT_3_FRAME_ID_TOWN,             /* 47 */
  ID3v2_DOT_3_FRAME_ID_TPE1,             /* 48 */
  ID3v2_DOT_3_FRAME_ID_TPE2,             /* 49 */
  ID3v2_DOT_3_FRAME_ID_TPE3,             /* 50 */
  ID3v2_DOT_3_FRAME_ID_TPE4,             /* 51 */
  ID3v2_DOT_3_FRAME_ID_TPOS,             /* 52 */
  ID3v2_DOT_3_FRAME_ID_TPUB,             /* 53 */
  ID3v2_DOT_3_FRAME_ID_TRCK,             /* 54 */
  ID3v2_DOT_3_FRAME_ID_TRDA,             /* 55 */
  ID3v2_DOT_3_FRAME_ID_TRSN,             /* 56 */
  ID3v2_DOT_3_FRAME_ID_TRSO,             /* 57 */
  ID3v2_DOT_3_FRAME_ID_TSIZ,             /* 58 */
  ID3v2_DOT_3_FRAME_ID_TSRC,             /* 59 */
  ID3v2_DOT_3_FRAME_ID_TSSE,             /* 60 */
  ID3v2_DOT_3_FRAME_ID_TYER,             /* 61 */
  ID3v2_DOT_3_FRAME_ID_TXXX,             /* 62 */
  ID3v2_DOT_3_FRAME_ID_UFID,             /* 63 */
  ID3v2_DOT_3_FRAME_ID_USER,             /* 64 */
  ID3v2_DOT_3_FRAME_ID_USLT,             /* 65 */
  ID3v2_DOT_3_FRAME_ID_WCOM,             /* 66 */
  ID3v2_DOT_3_FRAME_ID_WCOP,             /* 67 */
  ID3v2_DOT_3_FRAME_ID_WOAF,             /* 68 */
  ID3v2_DOT_3_FRAME_ID_WOAR,             /* 69 */
  ID3v2_DOT_3_FRAME_ID_WOAS,             /* 70 */
  ID3v2_DOT_3_FRAME_ID_WORS,             /* 71 */
  ID3v2_DOT_3_FRAME_ID_WPAY,             /* 72 */
  ID3v2_DOT_3_FRAME_ID_WPUB,             /* 73 */
  ID3v2_DOT_3_FRAME_ID_WXXX,             /* 74 */
};
#define ID3v2_DOT_3_KNOWN_FRAMES_NUMBER 74 /*((sizeof(known_id3v2_dot_3_frames))/(sizeof(id3v2_dot_3_frames_types_enum)))*/


int read_id3v2(mp3_file_info *file_inf , FILE *checked_file  , long int size ,  id3_fields_found *fields);



/*-----------------10/12/01 18:02-------------------
 * Internal data structures:
 * --------------------------------------------------*/
typedef struct id3v2frame_tag {
	struct id3v2frame_tag *next , *previous;
	unsigned long int frame_id;
	int id3v2_major_version;
//	long flags;
	char *fr_data;
	size_t fr_data_len;
}id3v2frame;

typedef struct id3v2_tag_type {
	id3v2frame *first_frame;
	id3v2frame *last_frame;
} id3v2_tag;

enum MY_FILE_INPUT_TYPE {
	MY_FILE_MEM,
	MY_FILE_FILE_HANDLER,
	MY_FILE_FILE_STREAM
};

typedef struct MY_FILE_TAG {
			enum MY_FILE_INPUT_TYPE stream_type;
			union  {
				struct  {
					void *data_start;
					void *current_point;
					size_t buffer_size;
				} mem_data;
				struct  {
					int handle;
					int current_position;
					bool got_eof;
				} file_handler_data;
				struct  {
					FILE *file;
				} file_stream_data;
			}big_u;
} MY_FILE;

#endif
