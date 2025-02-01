/*!
 * \file      soft-se.c
 *
 * \brief     Secure Element software implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2020 Semtech
 *
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 */
#include <stdlib.h>
#include <stdint.h>

#include "utilities.h"

#include "LoRaMacHeaderTypes.h"

#include "secure-element.h"
#include "secure-element-nvm.h"

#include "keys.h"

/* Copied unmodified from soft-se.c. This function depends on the other
 * secure element functions and can be abstracted completely from them.
 */

SecureElementStatus_t SecureElementProcessJoinAccept( JoinReqIdentifier_t joinReqType, uint8_t* joinEui,
                                                      uint16_t devNonce, uint8_t* encJoinAccept,
                                                      uint8_t encJoinAcceptSize, uint8_t* decJoinAccept,
                                                      uint8_t* versionMinor )
{
    if( ( encJoinAccept == NULL ) || ( decJoinAccept == NULL ) || ( versionMinor == NULL ) )
    {
        return SECURE_ELEMENT_ERROR_NPE;
    }

    // Check that frame size isn't bigger than a JoinAccept with CFList size
    if( encJoinAcceptSize > LORAMAC_JOIN_ACCEPT_FRAME_MAX_SIZE )
    {
        return SECURE_ELEMENT_ERROR_BUF_SIZE;
    }

    // Determine decryption key
    KeyIdentifier_t encKeyID = NWK_KEY;

    if( joinReqType != JOIN_REQ )
    {
        encKeyID = J_S_ENC_KEY;
    }

    memcpy1( decJoinAccept, encJoinAccept, encJoinAcceptSize );

    // Decrypt JoinAccept, skip MHDR
    if( SecureElementAesEncrypt( encJoinAccept + LORAMAC_MHDR_FIELD_SIZE, encJoinAcceptSize - LORAMAC_MHDR_FIELD_SIZE,
                                 encKeyID, decJoinAccept + LORAMAC_MHDR_FIELD_SIZE ) != SECURE_ELEMENT_SUCCESS )
    {
        return SECURE_ELEMENT_FAIL_ENCRYPT;
    }

    *versionMinor = ( ( decJoinAccept[11] & 0x80 ) == 0x80 ) ? 1 : 0;

    uint32_t mic = 0;

    mic = ( ( uint32_t ) decJoinAccept[encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE] << 0 );
    mic |= ( ( uint32_t ) decJoinAccept[encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE + 1] << 8 );
    mic |= ( ( uint32_t ) decJoinAccept[encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE + 2] << 16 );
    mic |= ( ( uint32_t ) decJoinAccept[encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE + 3] << 24 );

    //  - Header buffer to be used for MIC computation
    //        - LoRaWAN 1.0.x : micHeader = [MHDR(1)]
    //        - LoRaWAN 1.1.x : micHeader = [JoinReqType(1), JoinEUI(8), DevNonce(2), MHDR(1)]

    // Verify mic
    if( *versionMinor == 0 )
    {
        // For LoRaWAN 1.0.x
        //   cmac = aes128_cmac(NwkKey, MHDR |  JoinNonce | NetID | DevAddr | DLSettings | RxDelay | CFList |
        //   CFListType)
        if( SecureElementVerifyAesCmac( decJoinAccept, ( encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE ), mic, NWK_KEY ) !=
            SECURE_ELEMENT_SUCCESS )
        {
            return SECURE_ELEMENT_FAIL_CMAC;
        }
    }
#if( USE_LRWAN_1_1_X_CRYPTO == 1 )
    else if( *versionMinor == 1 )
    {
        uint8_t  micHeader11[JOIN_ACCEPT_MIC_COMPUTATION_OFFSET] = { 0 };
        uint16_t bufItr                                          = 0;

        micHeader11[bufItr++] = ( uint8_t ) joinReqType;

        memcpyr( micHeader11 + bufItr, joinEui, LORAMAC_JOIN_EUI_FIELD_SIZE );
        bufItr += LORAMAC_JOIN_EUI_FIELD_SIZE;

        micHeader11[bufItr++] = devNonce & 0xFF;
        micHeader11[bufItr++] = ( devNonce >> 8 ) & 0xFF;

        // For LoRaWAN 1.1.x and later:
        //   cmac = aes128_cmac(JSIntKey, JoinReqType | JoinEUI | DevNonce | MHDR | JoinNonce | NetID | DevAddr |
        //   DLSettings | RxDelay | CFList | CFListType)
        // Prepare the msg for integrity check (adding JoinReqType, JoinEUI and DevNonce)
        uint8_t localBuffer[LORAMAC_JOIN_ACCEPT_FRAME_MAX_SIZE + JOIN_ACCEPT_MIC_COMPUTATION_OFFSET] = { 0 };

        memcpy1( localBuffer, micHeader11, JOIN_ACCEPT_MIC_COMPUTATION_OFFSET );
        memcpy1( localBuffer + JOIN_ACCEPT_MIC_COMPUTATION_OFFSET - 1, decJoinAccept, encJoinAcceptSize );

        if( SecureElementVerifyAesCmac( localBuffer,
                                        encJoinAcceptSize + JOIN_ACCEPT_MIC_COMPUTATION_OFFSET -
                                            LORAMAC_MHDR_FIELD_SIZE - LORAMAC_MIC_FIELD_SIZE,
                                        mic, J_S_INT_KEY ) != SECURE_ELEMENT_SUCCESS )
        {
            return SECURE_ELEMENT_FAIL_CMAC;
        }
    }
#endif
    else
    {
        return SECURE_ELEMENT_ERROR_INVALID_LORAWAM_SPEC_VERSION;
    }

    return SECURE_ELEMENT_SUCCESS;
}

SecureElementStatus_t SecureElementInit( SecureElementNvmData_t* nvm ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementDeriveAndStoreKey( uint8_t* input, KeyIdentifier_t rootKeyID,
                                                      KeyIdentifier_t targetKeyID ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementAesEncrypt( uint8_t* buffer, uint16_t size, KeyIdentifier_t keyID,
                                               uint8_t* encBuffer ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementComputeAesCmac( uint8_t* micBxBuffer, uint8_t* buffer, uint16_t size,
                                                   KeyIdentifier_t keyID, uint32_t* cmac ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementVerifyAesCmac( uint8_t* buffer, uint16_t size, uint32_t expectedCmac,
                                                  KeyIdentifier_t keyID ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementSetKey( KeyIdentifier_t keyID, uint8_t* key ) {
        /* TODO */
        return SECURE_ELEMENT_ERROR;
}

SecureElementStatus_t SecureElementSetDevEui( uint8_t* devEui )
{
	/* Ignored */
	return SECURE_ELEMENT_SUCCESS;
}

uint8_t* SecureElementGetDevEui( void )
{
    	return get_dev_eui();
}

SecureElementStatus_t SecureElementSetJoinEui( uint8_t* joinEui )
{
	/* Ignored */
	return SECURE_ELEMENT_SUCCESS;
}

uint8_t* SecureElementGetJoinEui( void )
{
	return get_join_eui();
}

SecureElementStatus_t SecureElementSetPin( uint8_t* pin )
{
	/* Ignored */
	return SECURE_ELEMENT_SUCCESS;
}

uint8_t* SecureElementGetPin( void )
{
	LOG_ERR("Pin unsupported");
    	return NULL;
}
