#ifndef Authentication_h__
#define Authentication_h__

#include "DDSBuiltinSecurityCommon.h"
#include "DomainId_t.h"
#include "DomainParticipantQos.h"
#include "AuthenticationListener.h"
/** return code */
typedef enum ValidationResult_t
{
    /**  验证成功 */
    VALIDATION_OK,
    /**  验证失败 */
    VALIDATION_FAILED,
    /** 验证进行中，稍后重试*/
    VALIDATION_PENDING_RETRY,
    /** 需要发送握手信息 */
    VALIDATION_PENDING_HANDSHAKE_REQUEST,
    /** 验证处于挂起状态，DDS implementation 等待BuiltinParticipantMessageReader接受信息 */
    VALIDATION_PENDING_HANDSHAKE_MESSAGE,
    /** 验证成功，但是DDS implementation应该使用BuiltinParticipantMessageWriter发送消息 */
    VALIDATION_OK_FINAL_MESSAGE
}ValidationResult_t;

/** 用来独立验证domainparticipant*/
typedef DDSSecHandle IdentityHandle;
/** 描述鉴定或者握手协议的状态参数*/
typedef DDSSecHandle HandshakeHandle;
typedef DDSSecHandle SharedSecretHandle;

class Authentication
{

public:
    virtual ~Authentication(){};

    /**
     * @fn virtual ValidationResult_t validate_local_identity(
     *     IdentityHandle *local_identity_handle,
     *     DDS_BuiltinTopicKey_t adjusted_participant_key,
     *     const DDS_DomainId_t domain_id,
     *     const DDS_DomainParticipantQos *participant_qos,
     *     const DDS_BuiltinTopicKey_t candidate_participant_key,
     *     SecurityException *exception) = 0;
     *
     * @brief 验证当地DomainParticipant的身份
     *
     * @author lin
     *
     * @param [out] local_identity_handle    用来本地验证participant.
     * @param [out] adjusted_participant_key 生成的participant key.
     * @param domain_id                      Domainparticipant的Domain id.
     * @param participant_qos                Domainparticipant带的DomianparticipantQos.
     * @param candidate_participant_key      DDS implementation用来验证domainparticipant使用的key.
     * @param [out] exception                对于异常的描述.
     *
     * @return 表明操作成功或失败的值.
     */
    virtual ValidationResult_t validate_local_identity(
        IdentityHandle *local_identity_handle,
        DDS_BuiltinTopicKey_t adjusted_participant_key,
        const DDS_DomainId_t domain_id,
        const DDS_DomainParticipantQos *participant_qos,
        const DDS_BuiltinTopicKey_t candidate_participant_key,
        SecurityException *exception) = 0;

    /**
     * @fn virtual ValidationResult_t validate_remote_identity(
     *     IdentityHandle *remote_identity_handle,
     *     const IdentityHandle local_identity_handle,
     *     IdentityToken *remote_identity_token,
     *     const DDS_BuiltinTopicKey_t remote_participant_key,
     *     SecurityException *exception) = 0;
     *
     * @brief 验证远端domainparticipant身份.
     *
     * @author lin
     *
     * @param [out] remote_identity_handle   为远端参与者创建的handle,用于本地验证.
     * @param local_identity_handle          本地验证获取的handle，参照函数validate_local_identity.
     * @param [in,out] remote_identity_token 接收的token,来自远端调用的函数get_identity_token
     * @param remote_participant_key         从远端获取的pariticipant key.
     * @param [out] exception                对于异常的描述.
     *
     * @return 表明操作成功或失败的值.
     */
    virtual	ValidationResult_t validate_remote_identity(
        IdentityHandle *remote_identity_handle,
        const IdentityHandle local_identity_handle,
        IdentityToken *remote_identity_token,
        const DDS_BuiltinTopicKey_t remote_participant_key,
        SecurityException *exception) = 0;

    /**
     * @fn virtual ValidationResult_t begin_handshake_request(
     *     HandshakeHandle *handshake_handle,
     *     HandshakeMessageToken *handshake_message_token,
     *     const IdentityHandle initiator_identity_handle,
     *     const IdentityHandle replier_identity_handle,
     *     const char* builtin_topic_data,
     *     ZR_UINT32 builtin_topic_data_length,
     *     SecurityException *exception) = 0;
     *
     * @brief 发起握手请求.
     *
     * @author lin
     *
     * @param [out] handshake_handle        握手请求的handle.
     * @param [out] handshake_message_token 握手信息标记.
     * @param initiator_identity_handle     发起handshake的handle.
     * @param replier_identity_handle       握手被动端的handle.
     * @param builtin_topic_data            参与者的内置topic数据.
     * @param builtin_topic_data_length     内置topic数据长度.
     * @param [out] exception               对于异常的描述.
     *
     * @return 表明操作成功或失败的值.
     */
    virtual ValidationResult_t begin_handshake_request(
        HandshakeHandle *handshake_handle,
        HandshakeMessageToken *handshake_message_token,
        const IdentityHandle initiator_identity_handle,
        const IdentityHandle replier_identity_handle,
        const char* builtin_topic_data,
        ZR_UINT32 builtin_topic_data_length,
        SecurityException *exception) = 0;

    /**
     * @fn virtual ValidationResult_t begin_handshake_reply(
     *     HandshakeHandle *handshake_handle,
     *     HandshakeMessageToken *handshake_message_out,
     *     HandshakeMessageToken *handshake_message_in,
     *     const IdentityHandle initiator_identity_handle,
     *     const IdentityHandle replier_identity_handle,
     *     const char* builtin_topic_data,
     *     ZR_UINT32 builtin_topic_data_length,
     *     SecurityException *exception) = 0;
     *
     * @brief 回复握手请求.
     *
     * @author lin
     *
     * @param [out] handshake_handle        握手的handle.
     * @param [out] handshake_message_out   传出的handshake标记.
     * @param [in,out] handshake_message_in 传入的handshake标记(远端传入的token).
     * @param initiator_identity_handle     发起握手的handle.
     * @param replier_identity_handle       握手被动端的handle.
     * @param builtin_topic_data            参与者的内置topic数据.
     * @param builtin_topic_data_length     内置topic数据长度.
     * @param [out] exception               对于异常的描述.
     *
     * @return 表明操作成功或失败的值.
     */
    virtual ValidationResult_t begin_handshake_reply(
        HandshakeHandle *handshake_handle,
        HandshakeMessageToken *handshake_message_out,
        HandshakeMessageToken *handshake_message_in,
        const IdentityHandle initiator_identity_handle,
        const IdentityHandle replier_identity_handle,
        const char* builtin_topic_data,
        ZR_UINT32 builtin_topic_data_length,
        SecurityException *exception) = 0;

    /**
     * @fn virtual ValidationResult_t process_handshake(
     *     HandshakeMessageToken *handshake_message_out,
     *     HandshakeMessageToken *handshake_message_in,
     *     const HandshakeHandle handshake_handle, SecurityException *exception) = 0;
     *
     * @brief 继续一个握手.
     *
     * @author lin
     *
     * @param [out] handshake_message_out   返回的信息.
     * @param [in,out] handshake_message_in 传入的信息.
     * @param handshake_handle              握手的handle.
     * @param [out] exception               对于异常的描述.
     *
     * @return 表明操作成功或失败的值.
     */
    virtual ValidationResult_t process_handshake(
        HandshakeMessageToken *handshake_message_out,
        HandshakeMessageToken *handshake_message_in,
        const HandshakeHandle handshake_handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual SharedSecretHandle get_shared_secret(
     *     const HandshakeHandle handshake_handle, SecurityException *exception) = 0;
     *
     * @brief 获取秘钥.
     *
     * @author lin
     *
     * @param handshake_handle 握手的handle.
     * @param [out] exception  对于异常的描述.
     *
     * @return SharedSecretHandle.
     */
    virtual SharedSecretHandle get_shared_secret(
        const HandshakeHandle handshake_handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool get_authenticated_peer_credential_token(
     *     AuthenticatedPeerCredentialToken *peer_credential_token,
     *     const HandshakeHandle handshake_handle, SecurityException *exception) = 0;
     *
     * @brief Gets authenticated peer credential token.
     *
     * @author lin
     *
     * @param [out] peer_credential_token The peer credential token.
     * @param handshake_handle            握手的handle.
     * @param [out] exception             对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool get_authenticated_peer_credential_token(
        AuthenticatedPeerCredentialToken *peer_credential_token,
        const HandshakeHandle handshake_handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool get_identity_token(
     *     IdentityToken *identity_token,
     *     const IdentityHandle handle,
     *     SecurityException *exception) = 0;
     *
     * @brief 获取IdentityToken.
     *
     * @author lin
     *
     * @param [out] identity_token 通过handle查找到的token.
     * @param handle               验证的标识,来自函数validate_local_identity
     * @param [out] exception      对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool get_identity_token(
        IdentityToken *identity_token,
        const IdentityHandle handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool set_permissions_credential_and_token(
     *     IdentityHandle handle,
     *     const PermissionsCredentialToken *permissions_credential_token,
     *     const PermissionsToken *permissions_token,
     *     SecurityException *exception) = 0;
     *
     * @brief Sets permissions credential and token.
     *
     * @author lin
     *
     * @param handle                       来自函数valid_local_identity.
     * @param permissions_credential_token The permissions credential token.
     * @param permissions_token            The permissions token.
     * @param [out] exception              对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool set_permissions_credential_and_token(
        IdentityHandle handle,
        const PermissionsCredentialToken *permissions_credential_token,
        const PermissionsToken *permissions_token,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool set_listener(
     *     AuthenticationListener *listener, SecurityException *exception) = 0;
     *
     * @brief 设置一个监听器.
     *
     * @author lin
     *
     * @param [in,out] listener 监听Authentication对象的监听器.
     * @param [out] exception   对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool set_listener(
        AuthenticationListener *listener,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool return_identity_token(
     *     IdentityToken *token, SecurityException *exception) = 0;
     *
     * @brief 销毁IdentityToken对象
     *
     * @author lin
     *
     * @param [in,out] token  销毁的IdentityToken.
     * @param [out] exception 对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool return_identity_token(
        IdentityToken *token,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool return_authenticated_peer_credential_token(
     *     AuthenticatedPeerCredentialToken *peer_credential_token,
     *     SecurityException *exception) = 0;
     *
     * @brief 销毁AuthenticatedPeerCredentialToken对象
     *
     * @author lin
     *
     * @param [in,out] peer_credential_token    销毁的AuthenticatedPeerCredentialToken,
     *                                          来自函数get_authenticated_peer_credential_token.
     * @param [out] exception                对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool return_authenticated_peer_credential_token(
        AuthenticatedPeerCredentialToken *peer_credential_token,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool return_handshake_handle(
     *     const HandshakeHandle handshake_handle, SecurityException *exception) = 0;
     *
     * @brief 销毁HandshakeHandle.
     *
     * @author lin
     *
     * @param handshake_handle 销毁的HandshakeHandle,来自函数begin_handshake_request或者begin_handshake_reply.
     * @param [out] exception  对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool return_handshake_handle(
        const HandshakeHandle handshake_handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool return_identity_handle(
     *     const IdentityHandle identity_handle, SecurityException *exception) = 0;
     *
     * @brief 销毁IdentityHandle对象
     *
     * @author lin
     *
     * @param identity_handle 销毁的IdentityHandle,来自validate_local_identity或者validate_remote_identity
     * @param [out] exception 对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual bool return_identity_handle(
        const IdentityHandle identity_handle,
        SecurityException *exception) = 0;

    /**
     * @fn virtual bool return_sharedsecret_handle(
     *     const SharedSecretHandle sharedsecret_handle, SecurityException *exception) = 0;
     *
     * @brief 销毁SharedSecretHandle对象
     *
     * @author lin
     *
     * @param sharedsecret_handle 销毁的SharedSecretHandle,来自函数get_shared_secret.
     * @param [out] exception     对于异常的描述.
     *
     * @return 成功返回true，失败返回false.
     */
    virtual  bool return_sharedsecret_handle(
        const SharedSecretHandle sharedsecret_handle,
        SecurityException *exception) = 0;

   /**
    * @fn virtual bool return_handshake_token(
    *     HandshakeMessageToken *handshake_token, SecurityException *exception) = 0;
    *
    * @brief 释放handshake_token
    *
    * @author lin
    *
    * @param [in,out] handshake_token 销毁的HandshakeMessageToken.
    * @param [in,out] exception       对于异常的描述..
    *
    * @return true if it succeeds, false if it fails.
    */
   virtual bool return_handshake_token(
       HandshakeMessageToken *handshake_token,
       SecurityException *exception) = 0;

};


#endif //Authentication_h__
