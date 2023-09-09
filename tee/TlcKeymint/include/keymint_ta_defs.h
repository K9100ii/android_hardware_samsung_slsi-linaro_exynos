/*
 * Copyright (C) 2014-2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Originally from AOSP
 * Modified by Trustonic. */

#ifndef KEYMINT_TA_DEFS_H
#define KEYMINT_TA_DEFS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * TagType classifies Tags in Tag.aidl into various groups of data.
 */
typedef enum {
    /** Invalid type, used to designate a tag as uninitialized. */
    KM_INVALID = 0 << 28,
    /** Enumeration value. */
    KM_ENUM = 1 << 28,
    /** Repeatable enumeration value. */
    KM_ENUM_REP = 2 << 28,
    /** 32-bit unsigned integer. */
    KM_UINT = 3 << 28,
    /** Repeatable 32-bit unsigned integer. */
    KM_UINT_REP = 4 << 28,
    /** 64-bit unsigned integer. */
    KM_ULONG = 5 << 28,
    /** 64-bit unsigned integer representing a date and time, in milliseconds since 1 Jan 1970. */
    KM_DATE = 6 << 28,
    /** Boolean.  If a tag with this type is present, the value is "true".  If absent, "false". */
    KM_BOOL = 7 << 28,
    /**
     * Byte string containing an arbitrary-length integer, in a two's-complement big-endian
     * ordering.  The byte array contains the minimum number of bytes needed to represent the
     * integer, including at least one sign bit (so zero encodes as the single byte 0x00.  This
     * matches the encoding of both java.math.BigInteger.toByteArray() and contents octets for an
     * ASN.1 INTEGER value (X.690 section 8.3).  Examples:
     * - value 65536 encodes as 0x01 0x00 0x00
     * - value 65535 encodes as 0x00 0xFF 0xFF
     * - value   255 encodes as 0x00 0xFF
     * - value     1 encodes as 0x01
     * - value     0 encodes as 0x00
     * - value    -1 encodes as 0xFF
     * - value  -255 encodes as 0xFF 0x01
     * - value  -256 encodes as 0xFF 0x00
     */
    KM_BIGNUM = 8 << 28,
    /** Byte string */
    KM_BYTES = 9 << 28,
    /** Repeatable 64-bit unsigned integer */
    KM_ULONG_REP = 10 << 28,
} keymaster_tag_type_t;

/**
 * Tag specifies various kinds of tags that can be set in KeyParameter to identify what kind of
 * data are stored in KeyParameter.
 */
typedef enum {
    /**
     * Tag::INVALID should never be set.  It means you hit an error.
     */
    KM_TAG_INVALID = KM_INVALID | 0,

    /**
     * Tag::PURPOSE specifies the set of purposes for which the key may be used.  Possible values
     * are defined in the KeyPurpose enumeration.
     *
     * This tag is repeatable; keys may be generated with multiple values, although an operation has
     * a single purpose.  When begin() is called to start an operation, the purpose of the operation
     * is specified.  If the purpose specified for the operation is not authorized by the key (the
     * key didn't have a corresponding Tag::PURPOSE provided during generation/import), the
     * operation must fail with ErrorCode::INCOMPATIBLE_PURPOSE.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_PURPOSE = KM_ENUM_REP | 1, /* keymaster_purpose_t. */

    /**
     * Tag::ALGORITHM specifies the cryptographic algorithm with which the key is used.  This tag
     * must be provided to generateKey and importKey, and must be specified in the wrapped key
     * provided to importWrappedKey.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_ALGORITHM = KM_ENUM | 2, /* keymaster_algorithm_t. */

    /**
     * Tag::KEY_SIZE specifies the size, in bits, of the key, measuring in the normal way for the
     * key's algorithm.  For example, for RSA keys, Tag::KEY_SIZE specifies the size of the public
     * modulus.  For AES keys it specifies the length of the secret key material.  For 3DES keys it
     * specifies the length of the key material, not counting parity bits (though parity bits must
     * be provided for import, etc.).  Since only three-key 3DES keys are supported, 3DES
     * Tag::KEY_SIZE must be 168.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_KEY_SIZE = KM_UINT | 3,

    /**
     * Tag::BLOCK_MODE specifies the block cipher mode(s) with which the key may be used.  This tag
     * is only relevant to AES and 3DES keys.  Possible values are defined by the BlockMode enum.
     *
     * This tag is repeatable for key generation/import.  For AES and 3DES operations the caller
     * must specify a Tag::BLOCK_MODE in the params argument of begin().  If the mode is missing or
     * the specified mode is not in the modes specified for the key during generation/import, the
     * operation must fail with ErrorCode::INCOMPATIBLE_BLOCK_MODE.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_BLOCK_MODE = KM_ENUM_REP | 4, /* keymaster_block_mode_t. */

    /**
     * Tag::DIGEST specifies the digest algorithms that may be used with the key to perform signing
     * and verification operations.  This tag is relevant to RSA, ECDSA and HMAC keys.  Possible
     * values are defined by the Digest enum.
     *
     * This tag is repeatable for key generation/import.  For signing and verification operations,
     * the caller must specify a digest in the params argument of begin().  If the digest is missing
     * or the specified digest is not in the digests associated with the key, the operation must
     * fail with ErrorCode::INCOMPATIBLE_DIGEST.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_DIGEST = KM_ENUM_REP | 5, /* keymaster_digest_t. */

    /**
     * Tag::PADDING specifies the padding modes that may be used with the key.  This tag is relevant
     * to RSA, AES and 3DES keys.  Possible values are defined by the PaddingMode enum.
     *
     * PaddingMode::RSA_OAEP and PaddingMode::RSA_PKCS1_1_5_ENCRYPT are used only for RSA
     * encryption/decryption keys and specify RSA OAEP padding and RSA PKCS#1 v1.5 randomized
     * padding, respectively.  PaddingMode::RSA_PSS and PaddingMode::RSA_PKCS1_1_5_SIGN are used
     * only for RSA signing/verification keys and specify RSA PSS padding and RSA PKCS#1 v1.5
     * deterministic padding, respectively.
     *
     * PaddingMode::NONE may be used with either RSA, AES or 3DES keys.  For AES or 3DES keys, if
     * PaddingMode::NONE is used with block mode ECB or CBC and the data to be encrypted or
     * decrypted is not a multiple of the AES block size in length, the call to finish() must fail
     * with ErrorCode::INVALID_INPUT_LENGTH.
     *
     * PaddingMode::PKCS7 may only be used with AES and 3DES keys, and only with ECB and CBC modes.
     *
     * In any case, if the caller specifies a padding mode that is not usable with the key's
     * algorithm, the generation or import method must return ErrorCode::INCOMPATIBLE_PADDING_MODE.
     *
     * This tag is repeatable.  A padding mode must be specified in the call to begin().  If the
     * specified mode is not authorized for the key, the operation must fail with
     * ErrorCode::INCOMPATIBLE_BLOCK_MODE.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_PADDING = KM_ENUM_REP | 6, /* keymaster_padding_t. */

    /**
     * Tag::CALLER_NONCE specifies that the caller can provide a nonce for nonce-requiring
     * operations.  This tag is boolean, so the possible values are true (if the tag is present) and
     * false (if the tag is not present).
     *
     * This tag is used only for AES and 3DES keys, and is only relevant for CBC, CTR and GCM block
     * modes.  If the tag is not present in a key's authorization list, implementations must reject
     * any operation that provides Tag::NONCE to begin() with ErrorCode::CALLER_NONCE_PROHIBITED.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_CALLER_NONCE = KM_BOOL | 7,

    /**
     * Tag::MIN_MAC_LENGTH specifies the minimum length of MAC that can be requested or verified
     * with this key for HMAC keys and AES keys that support GCM mode.
     *
     * This value is the minimum MAC length, in bits.  It must be a multiple of 8 bits.  For HMAC
     * keys, the value must be least 64 and no more than 512.  For GCM keys, the value must be at
     * least 96 and no more than 128.  If the provided value violates these requirements,
     * generateKey() or importKey() must return ErrorCode::UNSUPPORTED_MIN_MAC_LENGTH.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_MIN_MAC_LENGTH = KM_UINT | 8,

    // Tag 9 reserved

    /**
     * Tag::EC_CURVE specifies the elliptic curve.  Possible values are defined in the EcCurve
     * enumeration.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_EC_CURVE = KM_ENUM | 10, /* keymaster_ec_curve_t */

    /**
     * Tag::RSA_PUBLIC_EXPONENT specifies the value of the public exponent for an RSA key pair.
     * This tag is relevant only to RSA keys, and is required for all RSA keys.
     *
     * The value is a 64-bit unsigned integer that satisfies the requirements of an RSA public
     * exponent.  This value must be a prime number.  IKeyMintDevice implementations must support
     * the value 2^16+1 and may support other reasonable values.  If no exponent is specified or if
     * the specified exponent is not supported, key generation must fail with
     * ErrorCode::INVALID_ARGUMENT.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_RSA_PUBLIC_EXPONENT = KM_ULONG | 200,

    // Tag 201 reserved

    /**
     * Tag::INCLUDE_UNIQUE_ID is specified during key generation to indicate that an attestation
     * certificate for the generated key should contain an application-scoped and time-bounded
     * device-unique ID.  See Tag::UNIQUE_ID.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_INCLUDE_UNIQUE_ID = KM_BOOL | 202,

    /**
     * Tag::RSA_OAEP_MGF_DIGEST specifies the MGF1 digest algorithms that may be used with RSA
     * encryption/decryption with OAEP padding.  Possible values are defined by the Digest enum.
     *
     * This tag is repeatable for key generation/import.  RSA cipher operations with OAEP padding
     * must specify an MGF1 digest in the params argument of begin(). If this tag is missing or the
     * specified digest is not in the MGF1 digests associated with the key then begin operation must
     * fail with ErrorCode::INCOMPATIBLE_MGF_DIGEST.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_RSA_OAEP_MGF_DIGEST = KM_ENUM_REP | 203,

    // Tag 301 reserved

    /**
     * Tag::BOOTLOADER_ONLY specifies only the bootloader can use the key.
     *
     * Any attempt to use a key with Tag::BOOTLOADER_ONLY from the Android system must fail with
     * ErrorCode::INVALID_KEY_BLOB.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_BOOTLOADER_ONLY = KM_BOOL | 302,

    /**
     * Tag::ROLLBACK_RESISTANCE specifies that the key has rollback resistance, meaning that when
     * deleted with deleteKey() or deleteAllKeys(), the key is guaranteed to be permanently deleted
     * and unusable.  It's possible that keys without this tag could be deleted and then restored
     * from backup.
     *
     * This tag is specified by the caller during key generation or import to require.  If the
     * IKeyMintDevice cannot guarantee rollback resistance for the specified key, it must return
     * ErrorCode::ROLLBACK_RESISTANCE_UNAVAILABLE.  IKeyMintDevice implementations are not
     * required to support rollback resistance.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_ROLLBACK_RESISTANCE = KM_BOOL | 303,

    // Reserved for future use.
    KM_TAG_HARDWARE_TYPE = KM_ENUM | 304,

    /**
     * Keys tagged with EARLY_BOOT_ONLY may only be used during early boot, until
     * IKeyMintDevice::earlyBootEnded() is called.  Early boot keys may be created after
     * early boot.  Early boot keys may not be imported at all, if Tag::EARLY_BOOT_ONLY is
     * provided to IKeyMintDevice::importKey, the import must fail with
     * ErrorCode::EARLY_BOOT_ENDED.
     */
    KM_TAG_EARLY_BOOT_ONLY = KM_BOOL | 305,

    /**
     * Tag::ACTIVE_DATETIME specifies the date and time at which the key becomes active, in
     * milliseconds since Jan 1, 1970.  If a key with this tag is used prior to the specified date
     * and time, IKeyMintDevice::begin() must return ErrorCode::KEY_NOT_YET_VALID;
     *
     * Need not be hardware-enforced.
     */
    KM_TAG_ACTIVE_DATETIME = KM_DATE | 400,

    /**
     * Tag::ORIGINATION_EXPIRE_DATETIME specifies the date and time at which the key expires for
     * signing and encryption purposes.  After this time, any attempt to use a key with
     * KeyPurpose::SIGN or KeyPurpose::ENCRYPT provided to begin() must fail with
     * ErrorCode::KEY_EXPIRED.
     *
     * The value is a 64-bit integer representing milliseconds since January 1, 1970.
     *
     * Need not be hardware-enforced.
     */
    KM_TAG_ORIGINATION_EXPIRE_DATETIME = KM_DATE | 401,

    /**
     * Tag::USAGE_EXPIRE_DATETIME specifies the date and time at which the key expires for
     * verification and decryption purposes.  After this time, any attempt to use a key with
     * KeyPurpose::VERIFY or KeyPurpose::DECRYPT provided to begin() must fail with
     * ErrorCode::KEY_EXPIRED.
     *
     * The value is a 64-bit integer representing milliseconds since January 1, 1970.
     *
     * Need not be hardware-enforced.
     */
    KM_TAG_USAGE_EXPIRE_DATETIME = KM_DATE | 402,

    /**
     * TODO(seleneh) this tag need to be deleted.
     *
     * TODO(seleneh) this tag need to be deleted.
     *
     * Tag::MIN_SECONDS_BETWEEN_OPS specifies the minimum amount of time that elapses between
     * allowed operations using a key.  This can be used to rate-limit uses of keys in contexts
     * where unlimited use may enable brute force attacks.
     *
     * The value is a 32-bit integer representing seconds between allowed operations.
     *
     * When a key with this tag is used in an operation, the IKeyMintDevice must start a timer
     * during the finish() or abort() call.  Any call to begin() that is received before the timer
     * indicates that the interval specified by Tag::MIN_SECONDS_BETWEEN_OPS has elapsed must fail
     * with ErrorCode::KEY_RATE_LIMIT_EXCEEDED.  This implies that the IKeyMintDevice must keep a
     * table of use counters for keys with this tag.  Because memory is often limited, this table
     * may have a fixed maximum size and KeyMint may fail operations that attempt to use keys with
     * this tag when the table is full.  The table must accommodate at least 8 in-use keys and
     * aggressively reuse table slots when key minimum-usage intervals expire.  If an operation
     * fails because the table is full, KeyMint returns ErrorCode::TOO_MANY_OPERATIONS.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_MIN_SECONDS_BETWEEN_OPS = KM_UINT | 403,

    /**
     * Tag::MAX_USES_PER_BOOT specifies the maximum number of times that a key may be used between
     * system reboots.  This is another mechanism to rate-limit key use.
     *
     * The value is a 32-bit integer representing uses per boot.
     *
     * When a key with this tag is used in an operation, a key-associated counter must be
     * incremented during the begin() call.  After the key counter has exceeded this value, all
     * subsequent attempts to use the key must fail with ErrorCode::MAX_OPS_EXCEEDED, until the
     * device is restarted.  This implies that the IKeyMintDevice must keep a table of use
     * counters for keys with this tag.  Because KeyMint memory is often limited, this table can
     * have a fixed maximum size and KeyMint can fail operations that attempt to use keys with
     * this tag when the table is full.  The table needs to accommodate at least 8 keys.  If an
     * operation fails because the table is full, IKeyMintDevice must
     * ErrorCode::TOO_MANY_OPERATIONS.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_MAX_USES_PER_BOOT = KM_UINT | 404,

    /**
     * Tag::USAGE_COUNT_LIMIT specifies the number of times that a key may be used. This can be
     * used to limit the use of a key.
     *
     * The value is a 32-bit integer representing the current number of attempts left.
     *
     * When initializing a limited use key, the value of this tag represents the maximum usage
     * limit for that key. After the key usage is exhausted, the key blob should be invalidated by
     * finish() call. Any subsequent attempts to use the key must result in a failure with
     * ErrorCode::INVALID_KEY_BLOB returned by IKeyMintDevice.
     *
     * At this point, if the caller specifies count > 1, it is not expected that any TEE will be
     * able to enforce this feature in the hardware due to limited resources of secure
     * storage. In this case, the tag with the value of maximum usage must be added to the key
     * characteristics with SecurityLevel::KEYSTORE by the IKeyMintDevice.
     *
     * On the other hand, if the caller specifies count = 1, some TEEs may have the ability
     * to enforce this feature in the hardware with its secure storage. If the IKeyMintDevice
     * implementation can enforce this feature, the tag with value = 1 must be added to the key
     * characteristics with the SecurityLevel of the IKeyMintDevice. If the IKeyMintDevice can't
     * enforce this feature even when the count = 1, the tag must be added to the key
     * characteristics with the SecurityLevel::KEYSTORE.
     *
     * When the key is attested, this tag with the same value must also be added to the attestation
     * record. This tag must have the same SecurityLevel as the tag that is added to the key
     * characteristics.
     */
    KM_TAG_USAGE_COUNT_LIMIT = KM_UINT | 405,

    /**
     * Tag::USER_ID specifies the ID of the Android user that is permitted to use the key.
     *
     * Must not be hardware-enforced.
     */
    KM_TAG_USER_ID = KM_UINT | 501,

    /**
     * Tag::USER_SECURE_ID specifies that a key may only be used under a particular secure user
     * authentication state.  This tag is mutually exclusive with Tag::NO_AUTH_REQUIRED.
     *
     * The value is a 64-bit integer specifying the authentication policy state value which must be
     * present in the userId or authenticatorId field of a HardwareAuthToken provided to begin(),
     * update(), or finish().  If a key with Tag::USER_SECURE_ID is used without a HardwareAuthToken
     * with the matching userId or authenticatorId, the IKeyMintDevice must return
     * ErrorCode::KEY_USER_NOT_AUTHENTICATED.
     *
     * Tag::USER_SECURE_ID interacts with Tag::AUTH_TIMEOUT in a very important way.  If
     * Tag::AUTH_TIMEOUT is present in the key's characteristics then the key is a "timeout-based"
     * key, and may only be used if the difference between the current time when begin() is called
     * and the timestamp in the HardwareAuthToken is less than the value in Tag::AUTH_TIMEOUT * 1000
     * (the multiplier is because Tag::AUTH_TIMEOUT is in seconds, but the HardwareAuthToken
     * timestamp is in milliseconds).  Otherwise the IKeyMintDevice must return
     * ErrorCode::KEY_USER_NOT_AUTHENTICATED.
     *
     * If Tag::AUTH_TIMEOUT is not present, then the key is an "auth-per-operation" key.  In this
     * case, begin() must not require a HardwareAuthToken with appropriate contents.  Instead,
     * update() and finish() must receive a HardwareAuthToken with Tag::USER_SECURE_ID value in
     * userId or authenticatorId fields, and the current operation's operation handle in the
     * challenge field.  Otherwise the IKeyMintDevice must return
     * ErrorCode::KEY_USER_NOT_AUTHENTICATED.
     *
     * This tag is repeatable.  If repeated, and any one of the values matches the HardwareAuthToken
     * as described above, the key is authorized for use.  Otherwise the operation must fail with
     * ErrorCode::KEY_USER_NOT_AUTHENTICATED.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_USER_SECURE_ID = KM_ULONG_REP | 502,

    /**
     * Tag::NO_AUTH_REQUIRED specifies that no authentication is required to use this key.  This tag
     * is mutually exclusive with Tag::USER_SECURE_ID.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_NO_AUTH_REQUIRED = KM_BOOL | 503,

    /**
     * Tag::USER_AUTH_TYPE specifies the types of user authenticators that may be used to authorize
     * this key.
     *
     * The value is one or more values from HardwareAuthenticatorType, ORed together.
     *
     * When IKeyMintDevice is requested to perform an operation with a key with this tag, it must
     * receive a HardwareAuthToken and one or more bits must be set in both the HardwareAuthToken's
     * authenticatorType field and the Tag::USER_AUTH_TYPE value.  That is, it must be true that
     *
     *    (token.authenticatorType & tag_user_auth_type) != 0
     *
     * where token.authenticatorType is the authenticatorType field of the HardwareAuthToken and
     * tag_user_auth_type is the value of Tag:USER_AUTH_TYPE.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_USER_AUTH_TYPE = KM_ENUM | 504,

    /**
     * Tag::AUTH_TIMEOUT specifies the time in seconds for which the key is authorized for use,
     * after user authentication.  If
     * Tag::USER_SECURE_ID is present and this tag is not, then the key requires authentication for
     * every usage (see begin() for the details of the authentication-per-operation flow).
     *
     * The value is a 32-bit integer specifying the time in seconds after a successful
     * authentication of the user specified by Tag::USER_SECURE_ID with the authentication method
     * specified by Tag::USER_AUTH_TYPE that the key can be used.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_AUTH_TIMEOUT = KM_UINT | 505,

    /**
     * Tag::ALLOW_WHILE_ON_BODY specifies that the key may be used after authentication timeout if
     * device is still on-body (requires on-body sensor).
     *
     * Cannot be hardware-enforced.
     */
    KM_TAG_ALLOW_WHILE_ON_BODY = KM_BOOL | 506,

    /**
     * TRUSTED_USER_PRESENCE_REQUIRED is an optional feature that specifies that this key must be
     * unusable except when the user has provided proof of physical presence.  Proof of physical
     * presence must be a signal that cannot be triggered by an attacker who doesn't have one of:
     *
     *    a) Physical control of the device or
     *
     *    b) Control of the secure environment that holds the key.
     *
     * For instance, proof of user identity may be considered proof of presence if it meets the
     * requirements.  However, proof of identity established in one security domain (e.g. TEE) does
     * not constitute proof of presence in another security domain (e.g. StrongBox), and no
     * mechanism analogous to the authentication token is defined for communicating proof of
     * presence across security domains.
     *
     * Some examples:
     *
     *     A hardware button hardwired to a pin on a StrongBox device in such a way that nothing
     *     other than a button press can trigger the signal constitutes proof of physical presence
     *     for StrongBox keys.
     *
     *     Fingerprint authentication provides proof of presence (and identity) for TEE keys if the
     *     TEE has exclusive control of the fingerprint scanner and performs fingerprint matching.
     *
     *     Password authentication does not provide proof of presence to either TEE or StrongBox,
     *     even if TEE or StrongBox does the password matching, because password input is handled by
     *     the non-secure world, which means an attacker who has compromised Android can spoof
     *     password authentication.
     *
     * Note that no mechanism is defined for delivering proof of presence to an IKeyMintDevice,
     * except perhaps as implied by an auth token.  This means that KeyMint must be able to check
     * proof of presence some other way.  Further, the proof of presence must be performed between
     * begin() and the first call to update() or finish().  If the first update() or the finish()
     * call is made without proof of presence, the keyMint method must return
     * ErrorCode::PROOF_OF_PRESENCE_REQUIRED and abort the operation.  The caller must delay the
     * update() or finish() call until proof of presence has been provided, which means the caller
     * must also have some mechanism for verifying that the proof has been provided.
     *
     * Only one operation requiring TUP may be in flight at a time.  If begin() has already been
     * called on one key with TRUSTED_USER_PRESENCE_REQUIRED, and another begin() comes in for that
     * key or another with TRUSTED_USER_PRESENCE_REQUIRED, KeyMint must return
     * ErrorCode::CONCURRENT_PROOF_OF_PRESENCE_REQUESTED.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_TRUSTED_USER_PRESENCE_REQUIRED = KM_BOOL | 507,

    /**
     * Tag::TRUSTED_CONFIRMATION_REQUIRED is only applicable to keys with KeyPurpose SIGN, and
     *  specifies that this key must not be usable unless the user provides confirmation of the data
     *  to be signed.  Confirmation is proven to keyMint via an approval token.  See
     *  CONFIRMATION_TOKEN, as well as the ConfirmationUI HAL.
     *
     * If an attempt to use a key with this tag does not have a cryptographically valid
     * CONFIRMATION_TOKEN provided to finish() or if the data provided to update()/finish() does not
     * match the data described in the token, keyMint must return NO_USER_CONFIRMATION.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_TRUSTED_CONFIRMATION_REQUIRED = KM_BOOL | 508,

    /**
     * Tag::UNLOCKED_DEVICE_REQUIRED specifies that the key may only be used when the device is
     * unlocked.
     *
     * Must be software-enforced.
     */
    KM_TAG_UNLOCKED_DEVICE_REQUIRED = KM_BOOL | 509,

    /**
     * Tag::APPLICATION_ID.  When provided to generateKey or importKey, this tag specifies data
     * that is necessary during all uses of the key.  In particular, calls to exportKey() and
     * getKeyCharacteristics() must provide the same value to the clientId parameter, and calls to
     * begin() must provide this tag and the same associated data as part of the inParams set.  If
     * the correct data is not provided, the method must return ErrorCode::INVALID_KEY_BLOB.
     *
     * The content of this tag must be bound to the key cryptographically, meaning it must not be
     * possible for an adversary who has access to all of the secure world secrets but does not have
     * access to the tag content to decrypt the key without brute-forcing the tag content, which
     * applications can prevent by specifying sufficiently high-entropy content.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_APPLICATION_ID = KM_BYTES | 601,

    /*
     * Semantically unenforceable tags, either because they have no specific meaning or because
     * they're informational only.
     */

    /**
     * Tag::APPLICATION_DATA.  When provided to generateKey or importKey, this tag specifies data
     * that is necessary during all uses of the key.  In particular, calls to begin() and
     * exportKey() must provide the same value to the appData parameter, and calls to begin must
     * provide this tag and the same associated data as part of the inParams set.  If the correct
     * data is not provided, the method must return ErrorCode::INVALID_KEY_BLOB.
     *
     * The content of this tag must be bound to the key cryptographically, meaning it must not be
     * possible for an adversary who has access to all of the secure world secrets but does not have
     * access to the tag content to decrypt the key without brute-forcing the tag content, which
     * applications can prevent by specifying sufficiently high-entropy content.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_APPLICATION_DATA = KM_BYTES | 700,

    /**
     * Tag::CREATION_DATETIME specifies the date and time the key was created, in milliseconds since
     * January 1, 1970.  This tag is optional and informational only, and not enforced by anything.
     *
     * Must be in the software-enforced list, if provided.
     */
    KM_TAG_CREATION_DATETIME = KM_DATE | 701,

    /**
     * Tag::ORIGIN specifies where the key was created, if known.  This tag must not be specified
     * during key generation or import, and must be added to the key characteristics by the
     * IKeyMintDevice.  The possible values are defined in the KeyOrigin enum.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_ORIGIN = KM_ENUM | 702, /* keymaster_key_origin_t. */

    // 703 is unused.

    /**
     * Tag::ROOT_OF_TRUST specifies the root of trust associated with the key used by verified boot
     * to validate the system.  It describes the boot key, verified boot state, boot hash, and
     * whether device is locked.  This tag is never provided to or returned from KeyMint in the
     * key characteristics.  It exists only to define the tag for use in the attestation record.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ROOT_OF_TRUST = KM_BYTES | 704,

    /**
     * Tag::OS_VERSION specifies the system OS version with which the key may be used.  This tag is
     * never sent to the IKeyMintDevice, but is added to the hardware-enforced authorization list
     * by the TA.  Any attempt to use a key with a Tag::OS_VERSION value different from the
     * currently-running OS version must cause begin(), getKeyCharacteristics() or exportKey() to
     * return ErrorCode::KEY_REQUIRES_UPGRADE.  See upgradeKey() for details.
     *
     * The value of the tag is an integer of the form MMmmss, where MM is the major version number,
     * mm is the minor version number, and ss is the sub-minor version number.  For example, for a
     * key generated on Android version 4.0.3, the value would be 040003.
     *
     * The IKeyMintDevice HAL must read the current OS version from the system property
     * ro.build.version.release and deliver it to the secure environment when the HAL is first
     * loaded (mechanism is implementation-defined).  The secure environment must not accept another
     * version until after the next boot.  If the content of ro.build.version.release has additional
     * version information after the sub-minor version number, it must not be included in
     * Tag::OS_VERSION.  If the content is non-numeric, the secure environment must use 0 as the
     * system version.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_OS_VERSION = KM_UINT | 705,

    /**
     * Tag::OS_PATCHLEVEL specifies the system security patch level with which the key may be used.
     * This tag is never sent to the keyMint TA, but is added to the hardware-enforced
     * authorization list by the TA.  Any attempt to use a key with a Tag::OS_PATCHLEVEL value
     * different from the currently-running system patchlevel must cause begin(),
     * getKeyCharacteristics() or exportKey() to return ErrorCode::KEY_REQUIRES_UPGRADE.  See
     * upgradeKey() for details.
     *
     * The value of the tag is an integer of the form YYYYMM, where YYYY is the four-digit year of
     * the last update and MM is the two-digit month of the last update.  For example, for a key
     * generated on an Android device last updated in December 2015, the value would be 201512.
     *
     * The IKeyMintDevice HAL must read the current system patchlevel from the system property
     * ro.build.version.security_patch and deliver it to the secure environment when the HAL is
     * first loaded (mechanism is implementation-defined).  The secure environment must not accept
     * another patchlevel until after the next boot.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_OS_PATCHLEVEL = KM_UINT | 706,

    /**
     * Tag::UNIQUE_ID specifies a unique, time-based identifier.  This tag is never provided to or
     * returned from KeyMint in the key characteristics.  It exists only to define the tag for use
     * in the attestation record.
     *
     * When a key with Tag::INCLUDE_UNIQUE_ID is attested, the unique ID is added to the attestation
     * record.  The value is a 128-bit hash that is unique per device and per calling application,
     * and changes monthly and on most password resets.  It is computed with:
     *
     *    HMAC_SHA256(T || C || R, HBK)
     *
     * Where:
     *
     *    T is the "temporal counter value", computed by dividing the value of
     *      Tag::CREATION_DATETIME by 2592000000, dropping any remainder.  T changes every 30 days
     *      (2592000000 = 30 * 24 * 60 * 60 * 1000).
     *
     *    C is the value of Tag::ATTESTATION_APPLICATION_ID that is provided to attested key
     *      generation/import operations.
     *
     *    R is 1 if Tag::RESET_SINCE_ID_ROTATION was provided to attested key generation/import or 0
     *      if the tag was not provided.
     *
     *    HBK is a unique hardware-bound secret known to the secure environment and never revealed
     *    by it.  The secret must contain at least 128 bits of entropy and be unique to the
     *    individual device (probabilistic uniqueness is acceptable).
     *
     *    HMAC_SHA256 is the HMAC function, with SHA-2-256 as the hash.
     *
     * The output of the HMAC function must be truncated to 128 bits.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_UNIQUE_ID = KM_BYTES | 707,

    /**
     * Tag::ATTESTATION_CHALLENGE is used to deliver a "challenge" value to the attested key
     * generation/import methods, which must place the value in the KeyDescription SEQUENCE of the
     * attestation extension.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_CHALLENGE = KM_BYTES | 708,

    /**
     * Tag::ATTESTATION_APPLICATION_ID identifies the set of applications which may use a key, used
     * only with attested key generation/import operations.
     *
     * The content of Tag::ATTESTATION_APPLICATION_ID is a DER-encoded ASN.1 structure, with the
     * following schema:
     *
     * AttestationApplicationId ::= SEQUENCE {
     *     packageInfoRecords SET OF PackageInfoRecord,
     *     signatureDigests   SET OF OCTET_STRING,
     * }
     *
     * PackageInfoRecord ::= SEQUENCE {
     *     packageName        OCTET_STRING,
     *     version            INTEGER,
     * }
     *
     * See system/security/keystore/keystore_attestation_id.cpp for details of construction.
     * IKeyMintDevice implementers do not need to create or parse the ASN.1 structure, but only
     * copy the tag value into the attestation record.  The DER-encoded string must not exceed 1 KiB
     * in length.
     *
     * Cannot be hardware-enforced.
     */
    KM_TAG_ATTESTATION_APPLICATION_ID = KM_BYTES | 709,

    /**
     * Tag::ATTESTATION_ID_BRAND provides the device's brand name, as returned by Build.BRAND in
     * Android, to attested key generation/import operations.  This field must be set only when
     * requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_BRAND = KM_BYTES | 710,

    /**
     * Tag::ATTESTATION_ID_DEVICE provides the device's device name, as returned by Build.DEVICE in
     * Android, to attested key generation/import operations.  This field must be set only when
     * requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_DEVICE = KM_BYTES | 711,

    /**
     * Tag::ATTESTATION_ID_PRODUCT provides the device's product name, as returned by Build.PRODUCT
     * in Android, to attested key generation/import operations.  This field must be set only when
     * requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_PRODUCT = KM_BYTES | 712,

    /**
     * Tag::ATTESTATION_ID_SERIAL the device's serial number.  This field must be set only when
     * requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_SERIAL = KM_BYTES | 713,

    /**
     * Tag::ATTESTATION_ID_IMEI provides the IMEIs for all radios on the device to attested key
     * generation/import operations.  This field must be set only when requesting attestation of the
     * device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_IMEI = KM_BYTES | 714,

    /**
     * Tag::ATTESTATION_ID_MEID provides the MEIDs for all radios on the device to attested key
     * generation/import operations.  This field must be set only when requesting attestation of the
     * device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_MEID = KM_BYTES | 715,

    /**
     * Tag::ATTESTATION_ID_MANUFACTURER provides the device's manufacturer name, as returned by
     * Build.MANUFACTURER in Android, to attested key generation/import operations.  This field must
     * be set only when requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_MANUFACTURER = KM_BYTES | 716,

    /**
     * Tag::ATTESTATION_ID_MODEL provides the device's model name, as returned by Build.MODEL in
     * Android, to attested key generation/import operations.  This field must be set only when
     * requesting attestation of the device's identifiers.
     *
     * If the device does not support ID attestation (or destroyAttestationIds() was previously
     * called and the device can no longer attest its IDs), any key attestation request that
     * includes this tag must fail with ErrorCode::CANNOT_ATTEST_IDS.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_ATTESTATION_ID_MODEL = KM_BYTES | 717,

    /**
     * Tag::VENDOR_PATCHLEVEL specifies the vendor image security patch level with which the key may
     * be used.  This tag is never sent to the keyMint TA, but is added to the hardware-enforced
     * authorization list by the TA.  Any attempt to use a key with a Tag::VENDOR_PATCHLEVEL value
     * different from the currently-running system patchlevel must cause begin(),
     * getKeyCharacteristics() or exportKey() to return ErrorCode::KEY_REQUIRES_UPGRADE.  See
     * upgradeKey() for details.
     *
     * The value of the tag is an integer of the form YYYYMMDD, where YYYY is the four-digit year of
     * the last update, MM is the two-digit month and DD is the two-digit day of the last
     * update.  For example, for a key generated on an Android device last updated on June 5, 2018,
     * the value would be 20180605.
     *
     * The IKeyMintDevice HAL must read the current vendor patchlevel from the system property
     * ro.vendor.build.security_patch and deliver it to the secure environment when the HAL is first
     * loaded (mechanism is implementation-defined).  The secure environment must not accept another
     * patchlevel until after the next boot.
     *
     * Must be hardware-enforced.
     */
    KM_TAG_VENDOR_PATCHLEVEL = KM_UINT | 718,

    /**
     * Tag::BOOT_PATCHLEVEL specifies the boot image (kernel) security patch level with which the
     * key may be used.  This tag is never sent to the keyMint TA, but is added to the
     * hardware-enforced authorization list by the TA.  Any attempt to use a key with a
     * Tag::BOOT_PATCHLEVEL value different from the currently-running system patchlevel must
     * cause begin(), getKeyCharacteristics() or exportKey() to return
     * ErrorCode::KEY_REQUIRES_UPGRADE.  See upgradeKey() for details.
     *
     * The value of the tag is an integer of the form YYYYMMDD, where YYYY is the four-digit year of
     * the last update, MM is the two-digit month and DD is the two-digit day of the last
     * update.  For example, for a key generated on an Android device last updated on June 5, 2018,
     * the value would be 20180605.  If the day is not known, 00 may be substituted.
     *
     * During each boot, the bootloader must provide the patch level of the boot image to the secure
     * environment (mechanism is implementation-defined).
     *
     * Must be hardware-enforced.
     */
    KM_TAG_BOOT_PATCHLEVEL = KM_UINT | 719,

    /**
     * DEVICE_UNIQUE_ATTESTATION is an argument to IKeyMintDevice::attested key generation/import
     * operations.  It indicates that attestation using a device-unique key is requested, rather
     * than a batch key.  When a device-unique key is used, the returned chain should contain two
     * certificates:
     *    * The attestation certificate, containing the attestation extension, as described in
            KeyCreationResult.aidl.
     *    * A self-signed root certificate, signed by the device-unique key.
     * No additional chained certificates are provided. Only SecurityLevel::STRONGBOX
     * IKeyMintDevices may support device-unique attestations.  SecurityLevel::TRUSTED_ENVIRONMENT
     * IKeyMintDevices must return ErrorCode::INVALID_ARGUMENT if they receive
     * DEVICE_UNIQUE_ATTESTATION.
     * SecurityLevel::STRONGBOX IKeyMintDevices need not support DEVICE_UNIQUE_ATTESTATION, and
     * return ErrorCode::CANNOT_ATTEST_IDS if they do not support it.
     *
     * The caller needs to obtain the device-unique keys out-of-band and compare them against the
     * key used to sign the self-signed root certificate.
     * To ease this process, the IKeyMintDevice implementation should include, both in the subject
     * and issuer fields of the self-signed root, the unique identifier of the device. Using the
     * unique identifier will make it straightforward for the caller to link a device to its key.
     *
     * IKeyMintDevice implementations that support device-unique attestation MUST add the
     * DEVICE_UNIQUE_ATTESTATION tag to device-unique attestations.
     */
    KM_TAG_DEVICE_UNIQUE_ATTESTATION = KM_BOOL | 720,

    /**
     * IDENTITY_CREDENTIAL_KEY is never used by IKeyMintDevice, is not a valid argument to key
     * generation or any operation, is never returned by any method and is never used in a key
     * attestation.  It is used in attestations produced by the IIdentityCredential HAL when that
     * HAL attests to Credential Keys.  IIdentityCredential produces KeyMint-style attestations.
     */
    KM_TAG_IDENTITY_CREDENTIAL_KEY = KM_BOOL | 721,

    /**
     * To prevent keys from being compromised if an attacker acquires read access to system / kernel
     * memory, some inline encryption hardware supports protecting storage encryption keys in
     * hardware without software having access to or the ability to set the plaintext keys.
     * Instead, software only sees wrapped version of these keys.
     *
     * STORAGE_KEY is used to denote that a key generated or imported is a key used for storage
     * encryption. Keys of this type can either be generated or imported or secure imported using
     * keyMint. exportKey() can be used to re-wrap storage key with a per-boot ephemeral key
     * wrapped key once the key characteristics are enforced.
     *
     * Keys with this tag cannot be used for any operation within keyMint.
     * ErrorCode::INVALID_OPERATION is returned when a key with Tag::STORAGE_KEY is provided to
     * begin().
     */
    KM_TAG_STORAGE_KEY = KM_BOOL | 722,

    /**
     * TODO: Delete when keystore1 is deleted.
     */
    KM_TAG_ASSOCIATED_DATA = KM_BYTES | 1000,

    /**
     * Tag::NONCE is used to provide or return a nonce or Initialization Vector (IV) for AES-GCM,
     * AES-CBC, AES-CTR, or 3DES-CBC encryption or decryption.  This tag is provided to begin during
     * encryption and decryption operations.  It is only provided to begin if the key has
     * Tag::CALLER_NONCE.  If not provided, an appropriate nonce or IV must be randomly generated by
     * KeyMint and returned from begin.
     *
     * The value is a blob, an arbitrary-length array of bytes.  Allowed lengths depend on the mode:
     * GCM nonces are 12 bytes in length; AES-CBC and AES-CTR IVs are 16 bytes in length, 3DES-CBC
     * IVs are 8 bytes in length.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_NONCE = KM_BYTES | 1001,

    /**
     * Tag::MAC_LENGTH provides the requested length of a MAC or GCM authentication tag, in bits.
     *
     * The value is the MAC length in bits.  It must be a multiple of 8 and at least as large as the
     * value of Tag::MIN_MAC_LENGTH associated with the key.  Otherwise, begin() must return
     * ErrorCode::INVALID_MAC_LENGTH.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_MAC_LENGTH = KM_UINT | 1003,

    /**
     * Tag::RESET_SINCE_ID_ROTATION specifies whether the device has been factory reset since the
     * last unique ID rotation.  Used for key attestation.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_RESET_SINCE_ID_ROTATION = KM_BOOL | 1004,

    /**
     * Tag::CONFIRMATION_TOKEN is used to deliver a cryptographic token proving that the user
     * confirmed a signing request.  The content is a full-length HMAC-SHA256 value.  See the
     * ConfirmationUI HAL for details of token computation.
     *
     * Must never appear in KeyCharacteristics.
     */
    KM_TAG_CONFIRMATION_TOKEN = KM_BYTES | 1005,

    /**
     * Tag::CERTIFICATE_SERIAL specifies the serial number to be assigned to the attestation
     * certificate to be generated for the given key.  This parameter should only be passed to
     * keyMint in the attestation parameters during generateKey() and importKey().  If not provided,
     * the serial shall default to 1.
     */
    KM_TAG_CERTIFICATE_SERIAL = KM_BIGNUM | 1006,

    /**
     * Tag::CERTIFICATE_SUBJECT the certificate subject.  The value is a DER encoded X509 NAME.
     * This value is used when generating a self signed certificates.  This tag may be specified
     * during generateKey and importKey. If not provided the subject name shall default to
     * CN="Android Keystore Key".
     */
    KM_TAG_CERTIFICATE_SUBJECT = KM_BYTES | 1007,

    /**
     * Tag::CERTIFICATE_NOT_BEFORE the beginning of the validity of the certificate in UNIX epoch
     * time in milliseconds.  This value is used when generating attestation or self signed
     * certificates.  ErrorCode::MISSING_NOT_BEFORE must be returned if this tag is not provided if
     * this tag is not provided to generateKey or importKey.
     */
    KM_TAG_CERTIFICATE_NOT_BEFORE = KM_DATE | 1008,

    /**
     * Tag::CERTIFICATE_NOT_AFTER the end of the validity of the certificate in UNIX epoch time in
     * milliseconds.  This value is used when generating attestation or self signed certificates.
     * ErrorCode::MISSING_NOT_AFTER must be returned if this tag is not provided to generateKey or
     * importKey.
     */
    KM_TAG_CERTIFICATE_NOT_AFTER = KM_DATE | 1009,

    /**
     * Tag::MAX_BOOT_LEVEL specifies a maximum boot level at which a key should function.
     *
     * Over the course of the init process, the boot level will be raised to
     * monotonically increasing integer values. Implementations MUST NOT allow the key
     * to be used once the boot level advances beyond the value of this tag.
     *
     * Cannot be hardware enforced in this version.
     */
    KM_TAG_MAX_BOOT_LEVEL = KM_UINT | 1010,

} keymaster_tag_t;

/**
 * Algorithms provided by IKeyMintDevice implementations.
 */
typedef enum {
    /** Asymmetric algorithms. */
    KM_ALGORITHM_RSA = 1,
    /** 2 removed, do not reuse. */
    KM_ALGORITHM_EC = 3,

    /** Block cipher algorithms */
    KM_ALGORITHM_AES = 32,
    KM_ALGORITHM_TRIPLE_DES = 33,

    /** MAC algorithms */
    KM_ALGORITHM_HMAC = 128,
} keymaster_algorithm_t;

/**
 * Symmetric block cipher modes provided by IKeyMintDevice implementations.
 */
typedef enum {
    /*
     * Unauthenticated modes, usable only for encryption/decryption and not generally recommended
     * except for compatibility with existing other protocols.
     */
    KM_MODE_ECB = 1,
    KM_MODE_CBC = 2,
    KM_MODE_CTR = 3,

    /*
     * Authenticated modes, usable for encryption/decryption and signing/verification.  Recommended
     * over unauthenticated modes for all purposes.
     */
    KM_MODE_GCM = 32,
} keymaster_block_mode_t;

/**
 * Padding modes that may be applied to plaintext for encryption operations.  This list includes
 * padding modes for both symmetric and asymmetric algorithms.  Note that implementations should not
 * provide all possible combinations of algorithm and padding, only the
 * cryptographically-appropriate pairs.
 */
typedef enum {
    KM_PAD_NONE = 1, /* deprecated */
    KM_PAD_RSA_OAEP = 2,
    KM_PAD_RSA_PSS = 3,
    KM_PAD_RSA_PKCS1_1_5_ENCRYPT = 4,
    KM_PAD_RSA_PKCS1_1_5_SIGN = 5,
    KM_PAD_PKCS7 = 64,
} keymaster_padding_t;

/**
 * Digests provided by keyMint implementations.
 */
typedef enum {
    KM_DIGEST_NONE = 0,
    KM_DIGEST_MD5 = 1,
    KM_DIGEST_SHA1 = 2,
    KM_DIGEST_SHA_2_224 = 3,
    KM_DIGEST_SHA_2_256 = 4,
    KM_DIGEST_SHA_2_384 = 5,
    KM_DIGEST_SHA_2_512 = 6,
} keymaster_digest_t;

/**
 * Supported EC curves, used in ECDSA
 */
typedef enum {
    KM_EC_CURVE_NONE  = -1,
    KM_EC_CURVE_P_224 = 0,
    KM_EC_CURVE_P_256 = 1,
    KM_EC_CURVE_P_384 = 2,
    KM_EC_CURVE_P_521 = 3,
    KM_EC_CURVE_25519 = 4,
} keymaster_ec_curve_t;

/**
 * The origin of a key (or pair), i.e. where it was generated.  Note that ORIGIN can be found in
 * either the hardware-enforced or software-enforced list for a key, indicating whether the key is
 * hardware or software-based.  Specifically, a key with GENERATED in the hardware-enforced list
 * must be guaranteed never to have existed outside the secure hardware.
 */
typedef enum {
    /** Generated in keyMint.  Should not exist outside the TEE. */
    KM_ORIGIN_GENERATED = 0,
    /** Derived inside keyMint.  Likely exists off-device. */
    KM_ORIGIN_DERIVED = 1,
    /** Imported into keyMint.  Existed as cleartext in Android. */
    KM_ORIGIN_IMPORTED = 2,
    /** Previously used for another purpose that is now obsolete. */
    KM_ORIGIN_RESERVED = 3,
    /**
     * Securely imported into KeyMint.  Was created elsewhere, and passed securely through Android
     * to secure hardware.
     */
    KM_ORIGIN_SECURELY_IMPORTED = 4,
} keymaster_key_origin_t;

/**
 * Possible purposes of a key (or pair).
 */
typedef enum {
    /* Usable with RSA, 3DES and AES keys. */
    KM_PURPOSE_ENCRYPT = 0,
    /* Usable with RSA, 3DES and AES keys. */
    KM_PURPOSE_DECRYPT = 1,
    /* Usable with RSA, EC and HMAC keys. */
    KM_PURPOSE_SIGN = 2,
    /* Usable with RSA, EC and HMAC keys. */
    KM_PURPOSE_VERIFY = 3,
    /* 4 is reserved */
    /* Usable with wrapping keys. */
    KM_PURPOSE_WRAP_KEY = 5,
    /* Key Agreement, usable with EC keys. */
    KM_PURPOSE_AGREE_KEY = 6,
    /* Usable as an attestation signing key.  Keys with this purpose must not have any other
     * purpose. */
    KM_PURPOSE_ATTEST_KEY = 7,
} keymaster_purpose_t;

typedef struct {
    const uint8_t* data;
    size_t data_length;
} keymaster_blob_t;

typedef struct {
    keymaster_tag_t tag;
    union {
        uint32_t enumerated;   /* KM_ENUM and KM_ENUM_REP */
        bool boolean;          /* KM_BOOL */
        uint32_t integer;      /* KM_INT and KM_INT_REP */
        uint64_t long_integer; /* KM_LONG */
        uint64_t date_time;    /* KM_DATE */
        keymaster_blob_t blob; /* KM_BIGNUM and KM_BYTES*/
    };
} keymaster_key_param_t;

typedef struct {
    keymaster_key_param_t* params; /* may be NULL if length == 0 */
    size_t length;
} keymaster_key_param_set_t;

/**
 * Parameters that define a key's characteristics, including authorized modes of usage and access
 * control restrictions.  The parameters are divided into two categories, those that are enforced by
 * secure hardware, and those that are not.  For a software-only keymint implementation the
 * enforced array must NULL.  Hardware implementations must enforce everything in the enforced
 * array.
 */
typedef struct {
    keymaster_key_param_set_t hw_enforced;
    keymaster_key_param_set_t sw_enforced;
} keymaster_key_characteristics_t;

typedef struct {
    const uint8_t* key_material;
    size_t key_material_size;
} keymaster_key_blob_t;

typedef struct {
    keymaster_blob_t* entries;
    size_t entry_count;
} keymaster_cert_chain_t;

typedef struct {
    keymaster_blob_t seed;
    uint8_t nonce[32];
} keymaster_hmac_sharing_parameters_t;

typedef struct {
    keymaster_hmac_sharing_parameters_t *params;
    size_t length;
} keymaster_hmac_sharing_parameters_set_t;

typedef enum {
    KM_VERIFIED_BOOT_VERIFIED = 0,    /* Full chain of trust extending from the bootloader to
                                       * verified partitions, including the bootloader, boot
                                       * partition, and all verified partitions*/
    KM_VERIFIED_BOOT_SELF_SIGNED = 1, /* The boot partition has been verified using the embedded
                                       * certificate, and the signature is valid. The bootloader
                                       * displays a warning and the fingerprint of the public
                                       * key before allowing the boot process to continue.*/
    KM_VERIFIED_BOOT_UNVERIFIED = 2,  /* The device may be freely modified. Device integrity is left
                                       * to the user to verify out-of-band. The bootloader
                                       * displays a warning to the user before allowing the boot
                                       * process to continue */
    KM_VERIFIED_BOOT_FAILED = 3,      /* The device failed verification. The bootloader displays a
                                       * warning and stops the boot process, so no keymint
                                       * implementation should ever actually return this value,
                                       * since it should not run.  Included here only for
                                       * completeness. */
} keymaster_verified_boot_t;

/**
 * Device security levels.  These enum values are used in two ways:
 *
 * 1.  Returned from IKeyMintDevice::getHardwareInfo to identify the security level of the
 *     IKeyMintDevice.  This characterizes the sort of environment in which the KeyMint
 *     implementation runs, and therefore the security of its operations.
 *
 * 2.  Associated with individual KeyMint authorization Tags in KeyCharacteristics or in attestation
 *     certificates.  This specifies the security level of the weakest environment involved in
 *     enforcing that particular tag, i.e. the sort of security environment an attacker would have
 *     to subvert in order to break the enforcement of that tag.
 */
typedef enum {
    /**
     * The SOFTWARE security level represents a KeyMint implementation that runs in an Android
     * process, or a tag enforced by such an implementation.  An attacker who can compromise that
     * process, or obtain root, or subvert the kernel on the device can defeat it.
     *
     * Note that the distinction between SOFTWARE and KEYSTORE is only relevant on-device.  For
     * attestation purposes, these categories are combined into the software-enforced authorization
     * list.
     */
    KM_SECURITY_LEVEL_SOFTWARE = 0,

    /**
     * The TRUSTED_ENVIRONMENT security level represents a KeyMint implementation that runs in an
     * isolated execution environment that is securely isolated from the code running on the kernel
     * and above, and which satisfies the requirements specified in CDD 9.11.1 [C-1-2]. An attacker
     * who completely compromises Android, including the Linux kernel, does not have the ability to
     * subvert it.  An attacker who can find an exploit that gains them control of the trusted
     * environment, or who has access to the physical device and can mount a sophisticated hardware
     * attack, may be able to defeat it.
     */
    KM_SECURITY_LEVEL_TRUSTED_ENVIRONMENT = 1,

    /**
     * The STRONGBOX security level represents a KeyMint implementation that runs in security
     * hardware that satisfies the requirements specified in CDD 9.11.2.  Roughly speaking, these
     * are discrete, security-focus computing environments that are hardened against physical and
     * side channel attack, and have had their security formally validated by a competent
     * penetration testing lab.
     */
    KM_SECURITY_LEVEL_STRONGBOX = 2,

    /**
     * KeyMint implementations must never return the KEYSTORE security level from getHardwareInfo.
     * It is used to specify tags that are not enforced by the IKeyMintDevice, but are instead
     * to be enforced by Keystore.  An attacker who can subvert the keystore process or gain root or
     * subvert the kernel can prevent proper enforcement of these tags.
     *
     *
     * Note that the distinction between SOFTWARE and KEYSTORE is only relevant on-device.  When
     * KeyMint generates an attestation certificate, these categories are combined into the
     * software-enforced authorization list.
     */
    KM_SECURITY_LEVEL_KEYSTORE = 100,
} keymaster_security_level_t;

/**
 * Hardware authentication type, used by HardwareAuthTokens to specify the mechanism used to
 * authentiate the user, and in KeyCharacteristics to specify the allowable mechanisms for
 * authenticating to activate a key.
 */
typedef enum {
    KM_HW_AUTH_TYPE_NONE = 0,
    KM_HW_AUTH_TYPE_PASSWORD = 1 << 0,
    KM_HW_AUTH_TYPE_FINGERPRINT = 1 << 1,
    // Additional entries must be powers of 2
    KM_HW_AUTH_TYPE_ANY = 0xFFFFFFFF,
} keymaster_hw_auth_type_t;

typedef struct {
    uint64_t challenge;
    uint64_t timestamp;
    uint32_t version; /* We call it version but it is hardcoded to 1 in AIDL */
    /**
     * 32-byte HMAC-SHA256 of the above values, computed as:
     *    HMAC(H,
     *         ISecureClock.TIME_STAMP_MAC_LABEL || challenge || timestamp || 1 )
     * where:
     *   ``ISecureClock.TIME_STAMP_MAC_LABEL'' is a string constant defined in ISecureClock.aidl.
     *   ``H'' is the shared HMAC key (see computeSharedHmac() in ISharedSecret).
     *   ``||'' represents concatenation
     * The representation of challenge and timestamp is as 64-bit unsigned integers in big-endian
     * order.  1, above, is a 32-bit unsigned integer, also big-endian.
     */
    uint8_t mac[32];
} keymaster_timestamp_token_t;

typedef struct {
    uint64_t challenge;
    uint64_t user_id;
    uint64_t authenticator_id;
    keymaster_hw_auth_type_t authenticator_type;
    uint64_t timestamp;
    keymaster_blob_t mac;
} keymaster_hw_auth_token_t;

/**
 * Formats for key import and export.
 */
typedef enum {
    /** X.509 certificate format, for public key export. */
    KM_KEY_FORMAT_X509 = 0,
    /** PKCS#8 format, asymmetric key pair import. */
    KM_KEY_FORMAT_PKCS8 = 1,
    /**
     * Raw bytes, for symmetric key import, and for import of raw asymmetric keys for curve 25519.
     */
    KM_KEY_FORMAT_RAW = 3,
} keymaster_key_format_t;

/**
 * The keymint operation API consists of begin, update, finish and abort. This is the type of the
 * handle used to tie the sequence of calls together.  A 64-bit value is used because it's important
 * that handles not be predictable.  Implementations must use strong random numbers for handle
 * values.
 */
typedef uint64_t keymaster_operation_handle_t;

typedef enum {
    KM_ERROR_OK = 0,
    KM_ERROR_ROOT_OF_TRUST_ALREADY_SET = -1,
    KM_ERROR_UNSUPPORTED_PURPOSE = -2,
    KM_ERROR_INCOMPATIBLE_PURPOSE = -3,
    KM_ERROR_UNSUPPORTED_ALGORITHM = -4,
    KM_ERROR_INCOMPATIBLE_ALGORITHM = -5,
    KM_ERROR_UNSUPPORTED_KEY_SIZE = -6,
    KM_ERROR_UNSUPPORTED_BLOCK_MODE = -7,
    KM_ERROR_INCOMPATIBLE_BLOCK_MODE = -8,
    KM_ERROR_UNSUPPORTED_MAC_LENGTH = -9,
    KM_ERROR_UNSUPPORTED_PADDING_MODE = -10,
    KM_ERROR_INCOMPATIBLE_PADDING_MODE = -11,
    KM_ERROR_UNSUPPORTED_DIGEST = -12,
    KM_ERROR_INCOMPATIBLE_DIGEST = -13,
    KM_ERROR_INVALID_EXPIRATION_TIME = -14,
    KM_ERROR_INVALID_USER_ID = -15,
    KM_ERROR_INVALID_AUTHORIZATION_TIMEOUT = -16,
    KM_ERROR_UNSUPPORTED_KEY_FORMAT = -17,
    KM_ERROR_INCOMPATIBLE_KEY_FORMAT = -18,
    KM_ERROR_UNSUPPORTED_KEY_ENCRYPTION_ALGORITHM = -19,   /* For PKCS8 & PKCS12 */
    KM_ERROR_UNSUPPORTED_KEY_VERIFICATION_ALGORITHM = -20, /* For PKCS8 & PKCS12 */
    KM_ERROR_INVALID_INPUT_LENGTH = -21,
    KM_ERROR_KEY_EXPORT_OPTIONS_INVALID = -22,
    KM_ERROR_DELEGATION_NOT_ALLOWED = -23,
    KM_ERROR_KEY_NOT_YET_VALID = -24,
    KM_ERROR_KEY_EXPIRED = -25,
    KM_ERROR_KEY_USER_NOT_AUTHENTICATED = -26,
    KM_ERROR_OUTPUT_PARAMETER_NULL = -27,
    KM_ERROR_INVALID_OPERATION_HANDLE = -28,
    KM_ERROR_INSUFFICIENT_BUFFER_SPACE = -29,
    KM_ERROR_VERIFICATION_FAILED = -30,
    KM_ERROR_TOO_MANY_OPERATIONS = -31,
    KM_ERROR_UNEXPECTED_NULL_POINTER = -32,
    KM_ERROR_INVALID_KEY_BLOB = -33,
    KM_ERROR_IMPORTED_KEY_NOT_ENCRYPTED = -34,
    KM_ERROR_IMPORTED_KEY_DECRYPTION_FAILED = -35,
    KM_ERROR_IMPORTED_KEY_NOT_SIGNED = -36,
    KM_ERROR_IMPORTED_KEY_VERIFICATION_FAILED = -37,
    KM_ERROR_INVALID_ARGUMENT = -38,
    KM_ERROR_UNSUPPORTED_TAG = -39,
    KM_ERROR_INVALID_TAG = -40,
    KM_ERROR_MEMORY_ALLOCATION_FAILED = -41,
    KM_ERROR_IMPORT_PARAMETER_MISMATCH = -44,
    KM_ERROR_SECURE_HW_ACCESS_DENIED = -45,
    KM_ERROR_OPERATION_CANCELLED = -46,
    KM_ERROR_CONCURRENT_ACCESS_CONFLICT = -47,
    KM_ERROR_SECURE_HW_BUSY = -48,
    KM_ERROR_SECURE_HW_COMMUNICATION_FAILED = -49,
    KM_ERROR_UNSUPPORTED_EC_FIELD = -50,
    KM_ERROR_MISSING_NONCE = -51,
    KM_ERROR_INVALID_NONCE = -52,
    KM_ERROR_MISSING_MAC_LENGTH = -53,
    KM_ERROR_KEY_RATE_LIMIT_EXCEEDED = -54,
    KM_ERROR_CALLER_NONCE_PROHIBITED = -55,
    KM_ERROR_KEY_MAX_OPS_EXCEEDED = -56,
    KM_ERROR_INVALID_MAC_LENGTH = -57,
    KM_ERROR_MISSING_MIN_MAC_LENGTH = -58,
    KM_ERROR_UNSUPPORTED_MIN_MAC_LENGTH = -59,
    KM_ERROR_UNSUPPORTED_KDF = -60,
    KM_ERROR_UNSUPPORTED_EC_CURVE = -61,
    KM_ERROR_KEY_REQUIRES_UPGRADE = -62,
    KM_ERROR_ATTESTATION_CHALLENGE_MISSING = -63,
    KM_ERROR_KEYMINT_NOT_CONFIGURED = -64,
    KM_ERROR_ATTESTATION_APPLICATION_ID_MISSING = -65,
    KM_ERROR_CANNOT_ATTEST_IDS = -66,
    KM_ERROR_ROLLBACK_RESISTANCE_UNAVAILABLE = -67,
    KM_ERROR_HARDWARE_TYPE_UNAVAILABLE = -68,
    KM_ERROR_PROOF_OF_PRESENCE_REQUIRED = -69,
    KM_ERROR_CONCURRENT_PROOF_OF_PRESENCE_REQUESTED = -70,
    KM_ERROR_NO_USER_CONFIRMATION = -71,
    KM_ERROR_DEVICE_LOCKED = -72,
    KM_ERROR_EARLY_BOOT_ENDED = -73,
    KM_ERROR_ATTESTATION_KEYS_NOT_PROVISIONED = -74,
    KM_ERROR_ATTESTATION_IDS_NOT_PROVISIONED = -75,
    KM_ERROR_INVALID_OPERATION = -76,
    KM_ERROR_STORAGE_KEY_UNSUPPORTED = -77,
    KM_ERROR_INCOMPATIBLE_MGF_DIGEST = -78,
    KM_ERROR_UNSUPPORTED_MGF_DIGEST = -79,
    KM_ERROR_MISSING_NOT_BEFORE = -80,
    KM_ERROR_MISSING_NOT_AFTER = -81,
    KM_ERROR_MISSING_ISSUER_SUBJECT = -82,
    KM_ERROR_INVALID_ISSUER_SUBJECT = -83,
    KM_ERROR_BOOT_LEVEL_EXCEEDED = -84,
    KM_ERROR_HARDWARE_NOT_YET_AVAILABLE = -85,

    KM_ERROR_UNIMPLEMENTED = -100,
    KM_ERROR_VERSION_MISMATCH = -101,

    KM_ERROR_UNKNOWN_ERROR = -1000,

    KM_ERROR_RKP_STATUS_FAILED                         = -100001,
    KM_ERROR_RKP_STATUS_INVALID_MAC                    = -100002,
    KM_ERROR_RKP_STATUS_PRODUCTION_KEY_IN_TEST_REQUEST = -100003,
    KM_ERROR_RKP_STATUS_TEST_KEY_IN_PRODUCTION_REQUEST = -100004,
    KM_ERROR_RKP_STATUS_INVALID_EEK                    = -100005,
} keymaster_error_t;

/* Convenience functions for manipulating keymint tag types */

static inline keymaster_tag_type_t keymaster_tag_get_type(keymaster_tag_t tag)
{
    return (keymaster_tag_type_t)(tag & (0xF << 28));
}

#ifndef TRUSTLET

inline void keymaster_free_param_values(keymaster_key_param_t* param, size_t param_count)
{
    while (param_count > 0) {
        param_count--;
        switch (keymaster_tag_get_type(param->tag)) {
            case KM_BIGNUM:
            case KM_BYTES:
                free((void*)param->blob.data);
                param->blob.data = NULL;
                break;
            default:
                // NOP
                break;
        }
        ++param;
    }
}

inline void keymaster_free_param_set(keymaster_key_param_set_t* set)
{
    if (set) {
        keymaster_free_param_values(set->params, set->length);
        free(set->params);
        set->params = NULL;
        set->length = 0;
    }
}

inline void keymaster_free_characteristics(keymaster_key_characteristics_t* characteristics)
{
    if (characteristics) {
        keymaster_free_param_set(&characteristics->hw_enforced);
        keymaster_free_param_set(&characteristics->sw_enforced);
    }
}

inline void keymaster_free_cert_chain(keymaster_cert_chain_t* chain)
{
    if (chain) {
        for (size_t i = 0; i < chain->entry_count; ++i) {
            free((uint8_t*)chain->entries[i].data);
            chain->entries[i].data = NULL;
            chain->entries[i].data_length = 0;
        }
        free(chain->entries);
        chain->entries = NULL;
        chain->entry_count = 0;
    }
}

#endif /* !TRUSTLET */

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // KEYMINT_TA_DEFS_H
