/*
 * $Id: as_crypt.h,v 1.14 2005/11/26 01:42:36 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_CRYPT_H_
#define __AS_CRYPT_H_

/*****************************************************************************/

typedef struct
{
	/* the key used to decrypt the 0x33 packet which is really the port the
	 * connection was made to */
	as_uint16 handshake_key;

	/* the seeds from the 0x33 handshake packet */
	as_uint16 session_seed_16;
	as_uint8  session_seed_8;
	
} ASCipher;

/*****************************************************************************/

/* allocate and init cipher */
ASCipher *as_cipher_create (as_uint16 handshake_key);

/* free cipher */
void as_cipher_free (ASCipher *cipher);

/* set session seeds */
void as_cipher_set_seeds (ASCipher *cipher, as_uint16 seed_16,
                          as_uint8 seed_8);

/*****************************************************************************/

/* encrypt/decrypt a block of data with session key */
void as_cipher_encrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len);

void as_cipher_decrypt (ASCipher *cipher, as_uint8 packet_seed,
                        as_uint8 *data, int len);

/* Encrypt entire packet using cipher. This will add the two bytes of seed
 * to the beginning of the packet.
 */
as_bool as_cipher_encrypt_packet (ASCipher *cipher, ASPacket *packet);

/* Decrypt entire packet using cipher. This will remove the two bytes of seed
 * at the beginning of the packet.
 */
as_bool as_cipher_decrypt_packet (ASCipher *cipher, ASPacket *packet);

/* encrypt/decrypt a block of data with handshake key */
void as_cipher_encrypt_handshake (ASCipher *cipher, as_uint8 *data, int len);
void as_cipher_decrypt_handshake (ASCipher *cipher, as_uint8 *data, int len);

/*****************************************************************************/

/* Calculate 22 byte nonce used in handshake from supernode GUID and session
 * seeds. Caller frees returned memory.
 */
as_uint8 *as_cipher_nonce (ASCipher *cipher, as_uint8 guid[16]);

/* Calculate 20 byte nonce used in handshake with Ares 2962 and later.
 * Requires supernode GUID from 0x38 packet. Caller frees returned memory.
 */
as_uint8 *as_cipher_nonce2 (as_uint8 guid[16]);

/*****************************************************************************/

/* Index nodes have their port derived from ip. Use this to calculate it. */
in_port_t as_ip2port (in_addr_t ip);

/*****************************************************************************/

/* encrypt/decrypt http download header b6st */
void as_encrypt_b6st (as_uint8 *data, int len);

void as_decrypt_b6st (as_uint8 *data, int len);

/* encrypt/decrypt http download header b6mi */
void as_encrypt_b6mi (as_uint8 *data, int len);

void as_decrypt_b6mi (as_uint8 *data, int len);

/* encrypt/decrypt push request */
void as_encrypt_push (as_uint8 *data, int len, in_addr_t host, in_port_t port);

void as_decrypt_push (as_uint8 *data, int len, in_addr_t host, in_port_t port);

/* encrypt/decrypt arlnk URLs */
void as_encrypt_arlnk (as_uint8 *data, int len);

void as_decrypt_arlnk (as_uint8 *data, int len);

/* encrypt/decrypt login string */
void as_encrypt_login_string (as_uint8 *data, int len, as_uint16 seed_16,
                              as_uint8 seed_8);

void as_decrypt_login_string (as_uint8 *data, int len, as_uint16 seed_16,
                              as_uint8 seed_8);

/*****************************************************************************/

/* encrypt/decrypt binary transfer requests */
as_bool as_encrypt_transfer_0a (ASPacket *packet);
as_bool as_decrypt_transfer_0a (ASPacket *packet);
as_bool as_encrypt_transfer_request (ASPacket *packet);
as_bool as_decrypt_transfer_request (ASPacket *packet);

/* encrypt/decrypt transfer replies. Updates key. */
as_bool as_encrypt_transfer_reply (ASPacket *packet, as_uint16 *key);
as_bool as_decrypt_transfer_reply (ASPacket *packet, as_uint16 *key);

/* encrypt/decrypt transfer body. Updates key. */
void as_encrypt_transfer_body (as_uint8 *data, int len, as_uint16 *key);
void as_decrypt_transfer_body (as_uint8 *data, int len, as_uint16 *key);


/*****************************************************************************/

#endif /* __AS_CRYPT_H_ */
