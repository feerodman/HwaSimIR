#ifndef CryCommonSource_h__
#define CryCommonSource_h__

#include "DDSBuiltinSecurityCommon.h"
#include "PropertyQosPolicy.h"

typedef DDSSecHandle DatawriterCryptoHandle;
typedef DDSSecHandle DatareaderCryptoHandle;
typedef DDSSecHandle ParticipantCryptoHandle;

typedef DDSSecHandle IdentityHandle;
typedef DDSSecHandle PermissionsHandle;

typedef DDSSecHandle SharedSecretHandle;


/**
* @typedef	struct DDS_SecureSubmessageCategorty_t
*
* @brief   信息类别数据结构.
*/

enum DDS_SecureSubmessageCategory_t
{
    DATAWRITER_SUBMESSAGE,
    DATAREADER_SUBMESSAGE,
    INFO_SUBMESSAGE,
    INVALID_SUBMESSAGE
};
//typedef struct DDS_SecureSubmessageCategorty_t
//{
//    
//
//}DDS_SecureSujmessageCategorty_t;

#endif // CryCommonSource_h__
