#ifndef CryptoKeyTransform_h__
#define CryptoKeyTransform_h__

#include "CryCommonSource.h"

class  CryptoTransform
{
public:

    virtual ~CryptoTransform(){};

    /**
    * @fn  bool CryptoTransform::encode_serialized_payload( ZR_INT8* encoded_buffer, ZR_INT32* encoded_buffer_length, ZR_INT8* extra_inline_qos, ZR_INT32* extra_inlien_qos_num, const ZR_INT8* plain_buffer, ZR_INT32 plain_buffer_length, DatawriterCryptoHandle sending_datawriter_crypto, SecurityException* exception);
    *
    * @brief   使用密钥对payload级别明文进行加密或签名
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] encoded_buffer          加密之后的密文.
    * @param [out] extra_inline_qos        加密产生的额外信息
    * @param   plain_buffer                需要加密的明文缓存首地址.
    * @param   plain_buffer_count          需要加密的明文缓存数量.
    * @param   sending_datawriter_crypto   发送端datawriter的密钥.
    * @param [out] exception               函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool encode_serialized_payload(
        ZRBuffer* encoded_buffer,
        ZRBuffer* extra_inline_qos,
        const ZRBuffer* plain_buffer,
        ZR_INT32 plain_buffer_count,
        DatawriterCryptoHandle sending_datawriter_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::encode_datawriter_submessage( ZR_INT8* encode_rtps_submessage, ZR_INT32* encode_rtps_submessage_length, const ZR_INT8* plain_rtps_submessage, ZR_INT32 plain_rtps_submessage_length, DatawriterCryptoHandle sending_datawriter_crypto, const DatareaderCryptoHandle* receiving_datareader_crypto_list, ZR_INT32 receiving_datareader_crypto_num, SecurityException* exception);
    *
    * @brief   使用密钥对datawriter的submessage进行加密或签名

    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] encode_rtps_submessage          加密后的密文.
    * @param   plain_rtps_submessage               需要加密的明文缓存.
    * @param   plain_rtps_submessage_count         需要加密的明文缓存数量.
    * @param   sending_datawriter_crypto           发送端datawriter的密钥.
    * @param   receiving_datareader_crypto_list    接受端datareader的密钥列表.
    * @param   receiving_datareader_crypto_num     接受端列表元素的数量.
    * @param [out] exception                       函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool encode_datawriter_submessage(
        ZRBuffer* encoded_rtps_submessage,
        const ZRBuffer* plain_rtps_submessage,
        ZR_INT32 plain_rtps_submessage_count,
        DatawriterCryptoHandle sending_datawriter_crypto,
        const DatareaderCryptoHandle* receiving_datareader_crypto_list,
        ZR_INT32 receiving_datareader_crypto_num,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::encode_datareader_submessage( ZR_INT8* encoded_rtps_submessage, ZR_INT32* encode_rtps_submessage_length, const ZR_INT8* plain_rtps_submessage, ZR_INT32 plain_rtps_suibmessage_length, DatareaderCryptoHandle sending_datareader_crypto, const DatawriterCryptoHandle* receiving_datawriter_crypto_list, ZR_INT32 receiving_datawriter_crypto_list_num, SecurityException* exception);
    *
    * @brief   根据datareader的密钥对datareader的submessage信息进行加密或者签名
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] encoded_rtps_submessage             加密之后的密文.
    * @param   plain_rtps_submessage                   需要加密的明文缓存.
    * @param   plain_rtps_suibmessage_count            需要加密的明文缓存数量.
    * @param   sending_datareader_crypto               发送端datareader的密钥.
    * @param   receiving_datawriter_crypto_list        接收端的datawriter密钥列表.
    * @param   receiving_datawriter_crypto_list_num    接收端列表元素的数量.
    * @param [out] exception                           函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool encode_datareader_submessage(
        ZRBuffer* encoded_rtps_submessage,
        const ZRBuffer* plain_rtps_submessage,
        ZR_INT32 plain_rtps_submessage_count,
        DatareaderCryptoHandle sending_datareader_crypto,
        const DatawriterCryptoHandle* receiving_datawriter_crypto_list,
        ZR_INT32 receiving_datawriter_crypto_list_num,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::encode_rtps_message( ZR_INT8* encoded_rtps_message, ZR_INT32* encoded_rtps_message_length, const ZR_INT8* plain_rtps_message, ZR_INT32 plain_rtps_message_length, ParticipantCryptoHandle sending_crypto, const ParticipantCryptoHandle* receiving_crypto_list, ZR_INT32 receiving_crypto_list_num, SecurityException* exception);
    *
    * @brief   使用participant的密钥对rtps的message进行加密或者签名
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] encoded_rtps_message        加密后的密文.
    * @param   plain_rtps_message              加密前的明文缓存.
    * @param   plain_rtps_message_count        加密前明文的缓存数量.
    * @param   sending_crypto                  发送端participant的密钥.
    * @param   receiving_crypto_list           接收端的participant密钥列表.
    * @param   receiving_crypto_list_num       接收端列表元素的数量.
    * @param [out] exception                   函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool encode_rtps_message(
        ZRBuffer* encoded_rtps_message,
        const ZRBuffer* plain_rtps_message,
        ZR_INT32 plain_rtps_message_count,
        ParticipantCryptoHandle sending_crypto,
        const ParticipantCryptoHandle* receiving_crypto_list,
        ZR_INT32 receiving_crypto_list_num,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::decode_rtps_message( ZR_INT8* plain_buffer, ZR_INT32* plain_buffer_length, const ZR_INT8* encoded_buffer, ZR_INT32 encoded_buffer_length, ParticipantCryptoHandle receiving_crypto, ParticipantCryptoHandle sending_crypto, SecurityException* exception);
    *
    * @brief   使用远端participant的密钥对远端的密文进行解密或者验证签名
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] plain_buffer        解密之后的明文.
    * @param   encoded_buffer          需要解密的密文缓存.
    * @param   encoded_buffer_count    需要解密的密文缓存数量.
    * @param   receiving_crypto        接收端的密钥.
    * @param   sending_crypto          发送端的密钥.
    * @param [out] exception           函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool decode_rtps_message(
        ZRBuffer* plain_buffer,
        const ZRBuffer* encoded_buffer,
        ZR_INT32 encoded_buffer_count,
        ParticipantCryptoHandle receiving_crypto,
        ParticipantCryptoHandle sending_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::preprocess_secure_submsg( DatawriterCryptoHandle datawriter_crypto, DatareaderCryptoHandle datareader_crypto, DDS_SecureSujmessageCategorty_t* secure_submessage_category, const ZR_INT8* encoded_rtps_submessage, ZR_INT32 encoded_rtps_submessage_length, ParticipantCryptoHandle receiving_crypto, ParticipantCryptoHandle sending_crypto, SecurityException* exception);
    *
    * @brief   为submessage寻找与之相匹配的datawriter密钥和datareader密钥
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] datawriter_crypto           datawriter的密钥.
    * @param [out] datareader_crypto           datareader的密钥.
    * @param [out] secure_submessage_category  submessage的信息类别.
    * @param   encoded_rtps_submessage         submessage的密文缓存.
    * @param   encoded_rtps_submessage_count   密文缓存数量.
    * @param   receiving_crypto                接收端participant的密钥.
    * @param   sending_crypto                  发送端participant的密钥.
    * @param [out] exception                   函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool preprocess_secure_submsg(
        DatawriterCryptoHandle* datawriter_crypto,
        DatareaderCryptoHandle* datareader_crypto,
        DDS_SecureSubmessageCategory_t* secure_submessage_category,
        const ZRBuffer* encoded_rtps_submessage,
        ZR_INT32 encoded_rtps_submessage_count,
        ParticipantCryptoHandle receiving_crypto,
        ParticipantCryptoHandle sending_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::decode_datawriter_submessage( ZR_INT8* plain_rtps_submessage, ZR_INT32* plain_rtps_submessage_length, const ZR_INT8* encoded_rtps_submessage, ZR_INT32 encoded_rtps_submessage_length, DatareaderCryptoHandle receiving_datareader_crypto, DatawriterCryptoHandle sending_datawriter_crypto, SecurityException* exception);
    *
    * @brief   使用远端datawriter的密钥对datawriter的submessage进行解密或者签名验证
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] plain_rtps_submessage           解密之后的明文.
    * @param   encoded_rtps_submessage             需要解密的密文缓存.
    * @param   encoded_rtps_submessage_count       需要解密的密文缓存数量.
    * @param   receiving_datareader_crypto         接收端datareader密钥.
    * @param   sending_datawriter_crypto           发送端datawriter密钥.
    * @param [out] exception                       函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool decode_datawriter_submessage(
        ZRBuffer* plain_rtps_submessage,
        const ZRBuffer* encoded_rtps_submessage,
        ZR_INT32 encoded_rtps_submessage_count,
        DatareaderCryptoHandle receiving_datareader_crypto,
        DatawriterCryptoHandle sending_datawriter_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::decode_datareader_submessage( ZR_INT8* plain_rtps_submessage, ZR_INT32* plain_rtps_submessage_length, const ZR_INT8* encoded_rtps_submessage, ZR_INT32 encoded_rtps_submessage_length, DatawriterCryptoHandle receiving_datawriter_crypto, DatareaderCryptoHandle sending_datareader_crypto, SecurityException* exception);
    *
    * @brief   使用远端datareader的密钥对datareader的submessage消息进行密文解密或者签名验证.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] plain_rtps_submessage           解密之后的明文.
    * @param   encoded_rtps_submessage             需要解密的密文缓存.
    * @param   encoded_rtps_submessage_count       需要解密的密文缓存数量.
    * @param   receiving_datawriter_crypto         接收端datawriter的密钥.
    * @param   sending_datareader_crypto           发送端datareader的密钥.
    * @param [out] exception                       函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool decode_datareader_submessage(
        ZRBuffer* plain_rtps_submessage,
        const ZRBuffer* encoded_rtps_submessage,
        ZR_INT32 encoded_rtps_submessage_count,
        DatawriterCryptoHandle receiving_datawriter_crypto,
        DatareaderCryptoHandle sending_datareader_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn  bool CryptoTransform::decode_serialized_payload( ZR_INT8* plain_buffer, ZR_INT32* plain_buffer_length, const ZR_INT8* encode_buffer, ZR_INT32 encode_buffer_length, const ZR_INT8* inline_qos, ZR_INT32 inline_qos_num, DatareaderCryptoHandle receiving_datareader_crypto, DatawriterCryptoHandle sending_datawriter_crypto, SecurityException* exception);
    *
    * @brief   使用秘钥对datareader的submessage消息进行密文解密或者签名验证.
    *
    * @author  Lxx
    * @date    2018/1/30
    *
    * @param [out] plain_buffer            解密后的明文.
    * @param   encode_buffer               需要解密的密文缓存.
    * @param   encode_buffer_count         需要解密密文缓存数量.
    * @param   inline_qos                  附带信息
    * @param   inline_qos_count            附带信息缓存数量.
    * @param   receiving_datareader_crypto 读取端的密钥.
    * @param   sending_datawriter_crypto   写入端的密钥.
    * @param [out] exception               函数执行失败时的错误信息.
    *
    * @return  成功则返回true， 失败返回false.
    */

    virtual bool decode_serialized_payload(
        ZRBuffer* plain_buffer,
        const ZRBuffer* encoded_buffer,
        ZR_INT32 encoded_buffer_count,
        const ZRBuffer* inline_qos,
        ZR_INT32 inline_qos_count,
        DatareaderCryptoHandle receiving_datareader_crypto,
        DatawriterCryptoHandle sending_datawriter_crypto,
        SecurityException* exception) = 0;

    /**
    * @fn virtual bool CryptoTransform::get_payload_encode_attributes( DatawriterCryptoHandle datawriter_crypto, CryptoPayloadEncodeAttributes* payload_encode_attributes) = 0;
    *
    * @brief 获取对数据负载进行加密的相关属性
    *
    * @author Tim
    * @date 18/3/29
    *
    * @param datawriter_crypto                  datawriter的密钥
    * @param [in,out] payload_encode_attributes 对数据负载进行加密的相关属性
    *
    * @return 成功则返回true， 失败返回false.
    */
    virtual bool get_payload_encode_attributes(
        DatawriterCryptoHandle datawriter_crypto,
        CryptoPayloadEncodeAttributes* payload_encode_attributes) = 0;

    /**
    * @fn virtual bool CryptoTransform::get_datawriter_submessage_encode_attributes( DatawriterCryptoHandle sending_datawriter_crypto, CryptoWriterSubmessageAttributes* writer_submessage_encode_attributes) = 0;
    *
    * @brief 获取对DataWriter子消息进行加密的相关属性
    *
    * @author Tim
    * @date 18/3/29
    *
    * @param sending_datawriter_crypto                    datawriter的密钥
    * @param [in,out] writer_submessage_encode_attributes  对DataWriter子消息进行加密的相关属性
    *
    * @return 成功则返回true， 失败返回false.
    */
    virtual bool get_datawriter_submessage_encode_attributes(
        DatawriterCryptoHandle sending_datawriter_crypto,
        CryptoWriterSubmessageAttributes* writer_submessage_encode_attributes) = 0;

    /**
    * @fn virtual bool CryptoTransform::get_datareader_submessage_encode_attributes( DatareaderCryptoHandle datareader_crypto, CryptoReaderSubmessageAttributes* reader_submessage_encode_attributes) = 0;
    *
    * @brief 获取对DataReader子消息进行加密的相关属性
    *
    * @author Tim
    * @date 18/3/29
    *
    * @param datareader_crypto                            datareader的密钥
    * @param [in,out] reader_submessage_encode_attributes  获取对DataReader子消息进行加密的相关属性
    *
    * @return 成功则返回true， 失败返回false.
    */
    virtual bool get_datareader_submessage_encode_attributes(
        DatareaderCryptoHandle datareader_crypto,
        CryptoReaderSubmessageAttributes* reader_submessage_encode_attributes) = 0;

    /**
    * @fn virtual bool CryptoTransform::get_rtps_message_encode_attributes( ParticipantCryptoHandle participant_crypto, CryptoRtpsMessageAttributes* rtps_message_encode_attributes) = 0;
    *
    * @brief 获取对RTPS消息进行加密的相关属性
    *
    * @author Tim
    * @date 18/3/29
    *
    * @param participant_crypto                      participant的密钥
    * @param [in,out] rtps_message_encode_attributes   获取对RTPS消息进行加密的相关属性
    *
    * @return 成功则返回true， 失败返回false.
    */
    virtual bool get_rtps_message_encode_attributes(
        ParticipantCryptoHandle participant_crypto,
        CryptoRtpsMessageAttributes* rtps_message_encode_attributes) = 0;
};
#endif // CryptoKeyTransform_h__
