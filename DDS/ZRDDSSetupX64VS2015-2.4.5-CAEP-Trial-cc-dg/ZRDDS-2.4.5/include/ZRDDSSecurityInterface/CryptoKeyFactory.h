#ifndef CryptoKeyFactory_h__
#define CryptoKeyFactory_h__


#include "CryCommonSource.h"

class CryptoKeyFactory
{
public:
    virtual ~CryptoKeyFactory(){};
    /**
    * @fn  ParticipantCryptoHandle CryptoKeyFactory::register_local_participant( IdentityHandle participant_identity, PermissionsHandle participant_permissions, PropertySeq participant_properties, SecurityException* exception);
    *
    * @brief   注册一个本地的participant加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   participant_identity    participant的标识.
    * @param   participant_permissions participant的许可信息.
    * @param   participant_properties  participant的固有属性.
    * @param [out] exception           发生错误时的错误返回值.
    *
    * @return  返回注册的Participant的加密句柄.
    */

    virtual ParticipantCryptoHandle register_local_participant(
        IdentityHandle  participant_identity,
        PermissionsHandle participant_permissions,
        const DDS_PropertySeq* participant_properties,
        SecurityException* exception) = 0;

    /**
    * @fn  ParticipantCryptoHandle CryptoKeyFactory::register_matched_remote_participant( ParticipantCryptoHandle remote_participant_identity, ParticipantCryptoHandle local_participant_crypto_handle, ShareSecret sharesecret, PermissionsHandle remote_participant_permissions, SecurityException* exception);
    *
    * @brief   为远端的participant注册一个加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   remote_participant_identity     远端participant的标识.
    * @param   local_participant_crypto_handle 与远端participant相匹配的本地participant的加密句柄.
    * @param   sharesecret                     共享秘钥.
    * @param   remote_participant_permissions  远端participant的许可信息.
    * @param [out] exception                   发生错误时的错误返回值.
    *
    * @return  返回注册的远端Participant的加密句柄.
    */

    virtual ParticipantCryptoHandle register_matched_remote_participant(
        IdentityHandle remote_participant_identity,
        ParticipantCryptoHandle local_participant_crypto_handle,
        SharedSecretHandle sharesecret,
        PermissionsHandle remote_participant_permissions,
        SecurityException* exception) = 0;

    /**
    * @fn  DatareaderCryptoHandle CryptoKeyFactory::register_local_datareader( ParticipantCryptoHandle participant_crypto, PropertySeq datareader_properties, SecurityException* exception);
    *
    * @brief   注册一个本地的datareader的加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   participant_crypto      datareader所属participant的加密句柄.
    * @param   datareader_properties   datareader的固有信息.
    * @param [out] exception           发生错误时的错误返回值.
    *
    * @return  返回注册的datareader的加密句柄.
    */

    virtual DatareaderCryptoHandle register_local_datareader(
        ParticipantCryptoHandle participant_crypto,
        const DDS_PropertySeq* datareader_properties,
        SecurityException* exception) = 0;

    /**
    * @fn  DatawriterCryptoHandle CryptoKeyFactory::register_local_datawriter( ParticipantCryptoHandle participant_crypto, PropertySeq datawriter_properties, SecurityException* exception);
    *
    * @brief   注册一个本地的datawriter的加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   participant_crypto      datawriter所属participant的加密句柄.
    * @param   datawriter_properties   datawriter的固有信息.
    * @param [out] exception           发生错误时的错误返回值.
    *
    * @return  返回注册的datawriter的加密句柄.
    */

    virtual DatawriterCryptoHandle register_local_datawriter(
        ParticipantCryptoHandle participant_crypto,
        const DDS_PropertySeq* datawriter_properties,
        SecurityException* exception) = 0;

    /**
    * @fn  DatareaderCryptoHandle CryptoKeyFactory::register_matched_remote_datareader( DatawriterCryptoHandle local_datawriter_crypto_handle, ParticipantCryptoHandle remote_participant_crypto, ShareSecret shared_secret, bool relay_only, SecurityException* exception);
    *
    * @brief   为远端的datareader注册一个加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   local_datawriter_crypto_handle  与远端相reader相匹配的本地writer的加密句柄.
    * @param   remote_participant_crypto       远端participant的加密句柄.
    * @param   shared_secret                   共享秘钥.
    * @param   relay_only                      是否只进行转发.
    * @param [out] exception                   发生错误时的错误返回值.
    *
    * @return  返回注册的datareader的加密句柄.
    */

    virtual DatareaderCryptoHandle register_matched_remote_datareader(
        DatawriterCryptoHandle local_datawriter_crypto_handle,
        ParticipantCryptoHandle remote_participant_crypto,
        SharedSecretHandle shared_secret,
        bool relay_only,
        SecurityException* exception) = 0;

    /**
    * @fn  DatawriterCryptoHandle CryptoKeyFactory::regiser_matched_remote_datawriter( DatawriterCryptoHandle local_datareader_crypto_handle, ParticipantCryptoHandle remote_participant_crypto, ShareSecret shared_secret, bool relay_only, SecurityException* exception);
    *
    * @brief   为远端的datawriter注册一个加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   local_datareader_crypto_handle  与远端writer相匹配的本地reader的加密句柄.
    * @param   remote_participant_crypto       远端participant的加密句柄.
    * @param   shared_secret                   共享秘钥.
    * @param [out] exception                   发生错误时的错误返回值.
    *
    * @return  返回注册的datawriter的加密句柄.
    */

    virtual DatawriterCryptoHandle register_matched_remote_datawriter(
        DatareaderCryptoHandle local_datareader_crypto_handle,
        ParticipantCryptoHandle remote_participant_crypto,
        SharedSecretHandle shared_secret,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoKeyFactory::unregister_participant( ParticipantCryptoHandle participant_crypto_handle, SecurityException* exception);
    *
    * @brief   销毁participant的加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   participant_crypto_handle   所需要销毁的participant句柄.
    * @param [out] exception               发生错误时的错误返回.
    *
    * @return  销毁成功则返回true，失败则返回false.
    */

    virtual bool unregister_participant(
        ParticipantCryptoHandle participant_crypto_handle,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoKeyFactory::unregister_datawriter( DatawriterCryptoHandle datawriter_crypto_handle, SecurityException* exception);
    *
    * @brief   销毁datawriter的加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   datawriter_crypto_handle    所需要销毁的datawriter句柄.
    * @param [out] exception               发生错误时的错误返回.
    *
    * @return  销毁成功则返回true，失败则返回false.
    */

    virtual bool unregister_datawriter(
        DatawriterCryptoHandle datawriter_crypto_handle,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoKeyFactory::unregister_datareader( DatareaderCryptoHandle datareader_crypto_handle, SecurityException* exception);
    *
    * @brief   销毁datareader的加密句柄.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param   datareader_crypto_handle    所需要销毁的datareader句柄.
    * @param [out] exception               发生错误时的错误返回.
    *
    * @return  销毁成功则返回true，失败则返回false.
    */

    virtual bool unregister_datareader(
        DatareaderCryptoHandle datareader_crypto_handle,
        SecurityException* exception) = 0;


};

#endif // CryptoKeyFactory_h__
