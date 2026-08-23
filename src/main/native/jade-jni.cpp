#include <jni.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nunchuk.h>

#include "deserializer.h"
#include "nunchukprovider.h"
#include "serializer.h"
#include "string-wrapper.h"
#include "utils/jade/jade.hpp"

namespace {

nunchuk::jade::JadeManager &manager() {
    auto *provider = NunchukProvider::get();
    if (!provider->nu || !provider->jadeManager) {
        throw std::runtime_error("Nunchuk must be initialized before using Jade");
    }
    return *provider->jadeManager;
}

std::string toString(JNIEnv *env, jstring value) {
    return StringWrapper(env, value);
}

nunchuk::Wallet parseWalletContent(
        JNIEnv *env,
        jstring wallet_content,
        jstring wallet_name) {
    auto wallet = nunchuk::Utils::ParseWalletDescriptor(toString(env, wallet_content));
    wallet.set_name(toString(env, wallet_name));
    return wallet;
}

std::vector<unsigned char> toBytes(JNIEnv *env, jbyteArray data) {
    std::vector<unsigned char> bytes;
    if (!data) {
        return bytes;
    }
    const auto size = env->GetArrayLength(data);
    bytes.resize(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte *>(bytes.data()));
    return bytes;
}

jbyteArray toByteArray(JNIEnv *env, const std::vector<unsigned char> &bytes) {
    auto array = env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (!bytes.empty()) {
        env->SetByteArrayRegion(
                array,
                0,
                static_cast<jsize>(bytes.size()),
                reinterpret_cast<const jbyte *>(bytes.data()));
    }
    return array;
}

jobject enumValue(JNIEnv *env, const char *class_name, int ordinal) {
    auto enum_class = env->FindClass(class_name);
    auto signature = std::string("()[L") + class_name + ";";
    auto values_method = env->GetStaticMethodID(enum_class, "values", signature.c_str());
    auto values = static_cast<jobjectArray>(env->CallStaticObjectMethod(enum_class, values_method));
    auto value = env->GetObjectArrayElement(values, ordinal);
    env->DeleteLocalRef(values);
    env->DeleteLocalRef(enum_class);
    return value;
}

int enumOrdinal(JNIEnv *env, jobject enum_value) {
    auto enum_class = env->GetObjectClass(enum_value);
    auto ordinal_method = env->GetMethodID(enum_class, "ordinal", "()I");
    auto ordinal = env->CallIntMethod(enum_value, ordinal_method);
    env->DeleteLocalRef(enum_class);
    return ordinal;
}

jobject toWriteList(
        JNIEnv *env,
        const std::vector<std::vector<unsigned char>> &writes) {
    auto list_class = env->FindClass("java/util/ArrayList");
    auto constructor = env->GetMethodID(list_class, "<init>", "()V");
    auto add_method = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    auto list = env->NewObject(list_class, constructor);
    for (const auto &write: writes) {
        auto array = toByteArray(env, write);
        env->CallBooleanMethod(list, add_method, array);
        env->DeleteLocalRef(array);
    }
    env->DeleteLocalRef(list_class);
    return list;
}

jobject toStringList(JNIEnv *env, const std::vector<std::string> &values) {
    auto list_class = env->FindClass("java/util/ArrayList");
    auto constructor = env->GetMethodID(list_class, "<init>", "()V");
    auto add_method = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    auto list = env->NewObject(list_class, constructor);
    for (const auto &value: values) {
        auto string = env->NewStringUTF(value.c_str());
        env->CallBooleanMethod(list, add_method, string);
        env->DeleteLocalRef(string);
    }
    env->DeleteLocalRef(list_class);
    return list;
}

int stepOrdinal(nunchuk::jade::JadeStepType type) {
    switch (type) {
        case nunchuk::jade::JadeStepType::WRITE:
            return 0;
        case nunchuk::jade::JadeStepType::READ_MORE:
            return 1;
        case nunchuk::jade::JadeStepType::CUSTOM_SERVER_APPROVAL:
            return 2;
        case nunchuk::jade::JadeStepType::COMPLETE:
            return 3;
        case nunchuk::jade::JadeStepType::FAILED:
            return 4;
    }
    return 4;
}

int interactionOrdinal(nunchuk::jade::UserInteraction interaction) {
    switch (interaction) {
        case nunchuk::jade::UserInteraction::NONE:
            return 0;
        case nunchuk::jade::UserInteraction::SETUP_DEVICE:
            return 1;
        case nunchuk::jade::UserInteraction::ENTER_PIN:
            return 2;
        case nunchuk::jade::UserInteraction::VERIFY_ADDRESS:
            return 3;
        case nunchuk::jade::UserInteraction::REGISTER_WALLET:
            return 4;
        case nunchuk::jade::UserInteraction::SIGN_MESSAGE:
            return 5;
        case nunchuk::jade::UserInteraction::SIGN_TRANSACTION:
            return 6;
        case nunchuk::jade::UserInteraction::APPROVE_PINSERVER:
            return 7;
    }
    return 0;
}

jobject toJadeError(JNIEnv *env, const std::optional<nunchuk::jade::JadeError> &error) {
    if (!error.has_value()) {
        return nullptr;
    }
    auto error_class = env->FindClass("com/nunchuk/android/jade/JadeError");
    auto constructor = env->GetMethodID(
            error_class,
            "<init>",
            "(ILjava/lang/String;ILjava/lang/String;)V");
    auto message = env->NewStringUTF(error->message.c_str());
    auto device_data = env->NewStringUTF(error->device_data.c_str());
    auto result = env->NewObject(
            error_class,
            constructor,
            static_cast<jint>(error->code),
            message,
            static_cast<jint>(error->device_code),
            device_data);
    env->DeleteLocalRef(device_data);
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(error_class);
    return result;
}

jobject toCustomPinServer(
        JNIEnv *env,
        const std::optional<nunchuk::jade::CustomPinServerInfo> &server) {
    if (!server.has_value()) {
        return nullptr;
    }
    auto server_class = env->FindClass("com/nunchuk/android/jade/JadeCustomPinServerInfo");
    auto constructor = env->GetMethodID(
            server_class,
            "<init>",
            "(Ljava/util/List;Ljava/lang/String;Ljava/lang/String;)V");
    auto urls = toStringList(env, server->urls);
    auto method = env->NewStringUTF(server->method.c_str());
    auto host = env->NewStringUTF(server->host.c_str());
    auto result = env->NewObject(server_class, constructor, urls, method, host);
    env->DeleteLocalRef(host);
    env->DeleteLocalRef(method);
    env->DeleteLocalRef(urls);
    env->DeleteLocalRef(server_class);
    return result;
}

jobject toJadeStep(JNIEnv *env, const nunchuk::jade::JadeStep &step) {
    auto step_class = env->FindClass("com/nunchuk/android/jade/JadeStep");
    auto constructor = env->GetMethodID(
            step_class,
            "<init>",
            "(Lcom/nunchuk/android/jade/JadeStepType;Lcom/nunchuk/android/jade/JadeUserInteraction;Ljava/util/List;Lcom/nunchuk/android/jade/JadeCustomPinServerInfo;Lcom/nunchuk/android/jade/JadeError;)V");
    auto type = enumValue(
            env,
            "com/nunchuk/android/jade/JadeStepType",
            stepOrdinal(step.type));
    auto interaction = enumValue(
            env,
            "com/nunchuk/android/jade/JadeUserInteraction",
            interactionOrdinal(step.interaction));
    auto writes = toWriteList(env, step.writes);
    auto custom_server = toCustomPinServer(env, step.custom_server);
    auto error = toJadeError(env, step.error);
    auto result = env->NewObject(
            step_class,
            constructor,
            type,
            interaction,
            writes,
            custom_server,
            error);
    if (error) env->DeleteLocalRef(error);
    if (custom_server) env->DeleteLocalRef(custom_server);
    env->DeleteLocalRef(writes);
    env->DeleteLocalRef(interaction);
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(step_class);
    return result;
}

int deviceStateOrdinal(nunchuk::jade::JadeDeviceState state) {
    switch (state) {
        case nunchuk::jade::JadeDeviceState::UNKNOWN:
            return 0;
        case nunchuk::jade::JadeDeviceState::LOCKED:
            return 1;
        case nunchuk::jade::JadeDeviceState::UNSAVED:
            return 2;
        case nunchuk::jade::JadeDeviceState::UNINITIALIZED:
            return 3;
        case nunchuk::jade::JadeDeviceState::TEMPORARY:
            return 4;
        case nunchuk::jade::JadeDeviceState::READY:
            return 5;
    }
    return 0;
}

int deviceNetworksOrdinal(nunchuk::jade::JadeDeviceNetworks networks) {
    switch (networks) {
        case nunchuk::jade::JadeDeviceNetworks::UNKNOWN:
            return 0;
        case nunchuk::jade::JadeDeviceNetworks::MAIN:
            return 1;
        case nunchuk::jade::JadeDeviceNetworks::TEST:
            return 2;
        case nunchuk::jade::JadeDeviceNetworks::ALL:
            return 3;
    }
    return 0;
}

jobject toOptionalLong(JNIEnv *env, const std::optional<uint32_t> &value) {
    if (!value.has_value()) {
        return nullptr;
    }
    auto long_class = env->FindClass("java/lang/Long");
    auto constructor = env->GetMethodID(long_class, "<init>", "(J)V");
    auto result = env->NewObject(long_class, constructor, static_cast<jlong>(*value));
    env->DeleteLocalRef(long_class);
    return result;
}

jobject toJadeDeviceInfo(JNIEnv *env, const nunchuk::jade::JadeDeviceInfo &device) {
    auto device_class = env->FindClass("com/nunchuk/android/jade/JadeDeviceInfo");
    auto constructor = env->GetMethodID(
            device_class,
            "<init>",
            "(Ljava/lang/String;JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Long;Lcom/nunchuk/android/jade/JadeDeviceState;Lcom/nunchuk/android/jade/JadeDeviceNetworks;ZZZ)V");
    auto firmware_version = env->NewStringUTF(device.firmware_version.c_str());
    auto config = env->NewStringUTF(device.config.c_str());
    auto board_type = env->NewStringUTF(device.board_type.c_str());
    auto features = env->NewStringUTF(device.features.c_str());
    auto idf_version = env->NewStringUTF(device.idf_version.c_str());
    auto chip_features = env->NewStringUTF(device.chip_features.c_str());
    auto efuse_mac = device.efuse_mac.has_value()
                     ? env->NewStringUTF(device.efuse_mac->c_str())
                     : nullptr;
    auto battery_status = toOptionalLong(env, device.battery_status);
    auto state = enumValue(
            env,
            "com/nunchuk/android/jade/JadeDeviceState",
            deviceStateOrdinal(device.state));
    auto networks = enumValue(
            env,
            "com/nunchuk/android/jade/JadeDeviceNetworks",
            deviceNetworksOrdinal(device.networks));
    auto result = env->NewObject(
            device_class,
            constructor,
            firmware_version,
            static_cast<jlong>(device.ota_max_chunk),
            config,
            board_type,
            features,
            idf_version,
            chip_features,
            efuse_mac,
            battery_status,
            state,
            networks,
            static_cast<jboolean>(device.has_pin),
            static_cast<jboolean>(device.has_pin_reported),
            static_cast<jboolean>(device.firmware_upgrade_required));
    env->DeleteLocalRef(networks);
    env->DeleteLocalRef(state);
    if (battery_status) env->DeleteLocalRef(battery_status);
    if (efuse_mac) env->DeleteLocalRef(efuse_mac);
    env->DeleteLocalRef(chip_features);
    env->DeleteLocalRef(idf_version);
    env->DeleteLocalRef(features);
    env->DeleteLocalRef(board_type);
    env->DeleteLocalRef(config);
    env->DeleteLocalRef(firmware_version);
    env->DeleteLocalRef(device_class);
    return result;
}

}  // namespace

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeCreateSession(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jint max_write_size) {
    try {
        if (max_write_size < 0) {
            throw std::invalid_argument("Jade maximum write size cannot be negative");
        }
        auto &session = manager().forSession(
                toString(env, session_id),
                static_cast<size_t>(max_write_size));
        return toJadeStep(env, session.initialize());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeConfirmCustomPinServer(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jboolean accepted) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.confirmCustomPinServer(accepted == JNI_TRUE));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetVersionInfo(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.getVersionInfo());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetExtendedPublicKey(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.getExtendedPublicKey(toString(env, derivation_path)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetMasterFingerprint(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.getMasterFingerprint());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeIsWalletRegistered(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.isWalletRegistered(
                Serializer::convert2CWallet(env, wallet)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeIsWalletRegisteredContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.isWalletRegistered(
                parseWalletContent(env, wallet_content, wallet_name)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeRegisterWallet(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.registerWallet(
                Serializer::convert2CWallet(env, wallet)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeRegisterWalletContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.registerWallet(
                parseWalletContent(env, wallet_content, wallet_name)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetWalletAddress(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet,
        jint address_index,
        jboolean change) {
    try {
        if (address_index < 0) {
            throw std::invalid_argument("Address index must be non-negative");
        }
        nunchuk::jade::WalletAddressOptions options;
        options.change = change == JNI_TRUE;
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.getWalletAddress(
                Serializer::convert2CWallet(env, wallet),
                static_cast<uint32_t>(address_index),
                options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetWalletAddressContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name,
        jint address_index,
        jboolean change) {
    try {
        if (address_index < 0) {
            throw std::invalid_argument("Address index must be non-negative");
        }
        nunchuk::jade::WalletAddressOptions options;
        options.change = change == JNI_TRUE;
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.getWalletAddress(
                parseWalletContent(env, wallet_content, wallet_name),
                static_cast<uint32_t>(address_index),
                options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeSignMessage(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path,
        jstring message) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.signMessage(
                toString(env, derivation_path),
                toString(env, message)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeSignPsbt(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet,
        jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.signPsbt(
                Serializer::convert2CWallet(env, wallet),
                toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeSignPsbtContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name,
        jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJadeStep(env, session.signPsbt(
                parseWalletContent(env, wallet_content, wallet_name),
                toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeOnData(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jbyteArray data) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        const auto bytes = toBytes(env, data);
        return toJadeStep(env, session.onData(bytes));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeIsInitialized(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        return static_cast<jboolean>(
                manager().forSession(toString(env, session_id)).initialized());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return JNI_FALSE;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetDeviceInfo(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto device = manager().forSession(toString(env, session_id)).deviceInfo();
        return toJadeDeviceInfo(env, device);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetExtendedPublicKeyResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::GetExtendedPublicKeyResult>();
        return env->NewStringUTF(result.extended_public_key.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetMasterFingerprintResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::GetMasterFingerprintResult>();
        return env->NewStringUTF(result.master_fingerprint.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetRegistrationResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::RegistrationResult>();
        return static_cast<jboolean>(result.registered);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return JNI_FALSE;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetWalletAddressResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::WalletAddressResult>();
        return env->NewStringUTF(result.address.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetMessageSignatureResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::SignMessageResult>();
        return env->NewStringUTF(result.signature.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_jadeGetSignPsbtResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::jade::SignPsbtResult>();
        return env->NewStringUTF(result.psbt.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}
