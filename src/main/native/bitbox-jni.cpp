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
#include "utils/bitbox/bitbox_manager.hpp"
#include "utils/bitbox/bitbox_session.hpp"
#include "utils/bitbox/types.hpp"

namespace {

nunchuk::bitbox::BitBoxManager &manager() {
    auto *provider = NunchukProvider::get();
    if (!provider->nu || !provider->bitBoxManager) {
        throw std::runtime_error("Nunchuk must be initialized before using BitBox");
    }
    return *provider->bitBoxManager;
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

int stepOrdinal(nunchuk::bitbox::BitBoxStepType type) {
    switch (type) {
        case nunchuk::bitbox::BitBoxStepType::WRITE:
            return 0;
        case nunchuk::bitbox::BitBoxStepType::READ_MORE:
            return 1;
        case nunchuk::bitbox::BitBoxStepType::RETRY_AFTER:
            return 2;
        case nunchuk::bitbox::BitBoxStepType::AWAITING_USER:
            return 3;
        case nunchuk::bitbox::BitBoxStepType::COMPLETE:
            return 4;
        case nunchuk::bitbox::BitBoxStepType::FAILED:
            return 5;
        default:
            return 5;
    }
}

int interactionOrdinal(nunchuk::bitbox::UserInteraction interaction) {
    switch (interaction) {
        case nunchuk::bitbox::UserInteraction::NONE:
            return 0;
        case nunchuk::bitbox::UserInteraction::UNLOCK_DEVICE:
            return 1;
        case nunchuk::bitbox::UserInteraction::CONFIRM_PAIRING:
            return 2;
        case nunchuk::bitbox::UserInteraction::VERIFY_ADDRESS:
            return 3;
        case nunchuk::bitbox::UserInteraction::REGISTER_WALLET:
            return 4;
        case nunchuk::bitbox::UserInteraction::SIGN_MESSAGE:
            return 5;
        case nunchuk::bitbox::UserInteraction::SIGN_TRANSACTION:
            return 6;
        case nunchuk::bitbox::UserInteraction::CONFIRM_DEVICE_NAME:
            return 7;
        case nunchuk::bitbox::UserInteraction::SET_DEVICE_PASSWORD:
            return 8;
        case nunchuk::bitbox::UserInteraction::SHOW_RECOVERY_WORDS:
            return 9;
        case nunchuk::bitbox::UserInteraction::INSERT_SD_CARD:
            return 10;
        case nunchuk::bitbox::UserInteraction::CREATE_BACKUP:
            return 11;
        case nunchuk::bitbox::UserInteraction::RESTORE_FROM_RECOVERY_WORDS:
            return 12;
        case nunchuk::bitbox::UserInteraction::RESTORE_FROM_BACKUP:
            return 13;
        case nunchuk::bitbox::UserInteraction::CHANGE_DEVICE_PASSWORD:
            return 14;
        case nunchuk::bitbox::UserInteraction::TOGGLE_MNEMONIC_PASSPHRASE:
            return 15;
        case nunchuk::bitbox::UserInteraction::CHECK_BACKUP:
            return 16;
        default:
            return 0;
    }
}

int productOrdinal(nunchuk::bitbox::BitBoxProduct product) {
    switch (product) {
        case nunchuk::bitbox::BitBoxProduct::UNKNOWN:
            return 0;
        case nunchuk::bitbox::BitBoxProduct::NOVA_MULTI:
            return 1;
        case nunchuk::bitbox::BitBoxProduct::NOVA_BITCOIN_ONLY:
            return 2;
        case nunchuk::bitbox::BitBoxProduct::BITBOX02_MULTI:
            return 3;
        case nunchuk::bitbox::BitBoxProduct::BITBOX02_BITCOIN_ONLY:
            return 4;
    }
    return 0;
}

int attestationOrdinal(nunchuk::bitbox::AttestationStatus status) {
    switch (status) {
        case nunchuk::bitbox::AttestationStatus::NOT_CHECKED:
            return 0;
        case nunchuk::bitbox::AttestationStatus::VALID:
            return 1;
        case nunchuk::bitbox::AttestationStatus::INVALID:
            return 2;
    }
    return 0;
}

jobject toBitBoxError(
        JNIEnv *env,
        const std::optional<nunchuk::bitbox::BitBoxError> &error) {
    if (!error.has_value()) {
        return nullptr;
    }
    auto error_class = env->FindClass("com/nunchuk/android/bitbox/BitBoxError");
    auto constructor = env->GetMethodID(error_class, "<init>", "(ILjava/lang/String;I)V");
    auto message = env->NewStringUTF(error->message.c_str());
    auto result = env->NewObject(
            error_class,
            constructor,
            static_cast<jint>(error->code),
            message,
            static_cast<jint>(error->device_code));
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(error_class);
    return result;
}

jstring toOptionalString(JNIEnv *env, const std::optional<std::string> &value) {
    return value.has_value() ? env->NewStringUTF(value->c_str()) : nullptr;
}

jobject toBitBoxStep(JNIEnv *env, const nunchuk::bitbox::BitBoxStep &step) {
    auto step_class = env->FindClass("com/nunchuk/android/bitbox/BitBoxStep");
    auto constructor = env->GetMethodID(
            step_class,
            "<init>",
            "(Lcom/nunchuk/android/bitbox/BitBoxStepType;Lcom/nunchuk/android/bitbox/BitBoxUserInteraction;Ljava/util/List;JLjava/lang/String;Ljava/lang/String;Lcom/nunchuk/android/bitbox/BitBoxError;)V");
    auto type = enumValue(
            env,
            "com/nunchuk/android/bitbox/BitBoxStepType",
            stepOrdinal(step.type));
    auto interaction = enumValue(
            env,
            "com/nunchuk/android/bitbox/BitBoxUserInteraction",
            interactionOrdinal(step.interaction));
    auto writes = toWriteList(env, step.writes);
    auto pairing_code = toOptionalString(env, step.pairing_code);
    auto warning = toOptionalString(env, step.warning);
    auto error = toBitBoxError(env, step.error);
    auto result = env->NewObject(
            step_class,
            constructor,
            type,
            interaction,
            writes,
            static_cast<jlong>(step.retry_after_ms),
            pairing_code,
            warning,
            error);
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(interaction);
    env->DeleteLocalRef(writes);
    if (pairing_code) env->DeleteLocalRef(pairing_code);
    if (warning) env->DeleteLocalRef(warning);
    if (error) env->DeleteLocalRef(error);
    env->DeleteLocalRef(step_class);
    return result;
}

jobject toBitBoxDeviceInfo(
        JNIEnv *env,
        const nunchuk::bitbox::BitBoxDeviceInfo &device) {
    auto device_class = env->FindClass("com/nunchuk/android/bitbox/BitBoxDeviceInfo");
    auto constructor = env->GetMethodID(
            device_class,
            "<init>",
            "(Lcom/nunchuk/android/bitbox/BitBoxProduct;Ljava/lang/String;Ljava/lang/String;ZZZLjava/lang/String;ZLjava/lang/String;Ljava/lang/String;)V");
    auto product = enumValue(
            env,
            "com/nunchuk/android/bitbox/BitBoxProduct",
            productOrdinal(device.product));
    auto firmware_version = env->NewStringUTF(device.firmware_version.c_str());
    auto name = env->NewStringUTF(device.name.c_str());
    auto securechip_model = env->NewStringUTF(device.securechip_model.c_str());
    auto bluetooth_firmware_version = env->NewStringUTF(
            device.bluetooth_firmware_version.c_str());
    auto bluetooth_firmware_hash = env->NewStringUTF(
            device.bluetooth_firmware_hash.c_str());
    auto result = env->NewObject(
            device_class,
            constructor,
            product,
            firmware_version,
            name,
            static_cast<jboolean>(device.unlocked),
            static_cast<jboolean>(device.initialized),
            static_cast<jboolean>(device.mnemonic_passphrase_enabled),
            securechip_model,
            static_cast<jboolean>(device.bluetooth_enabled),
            bluetooth_firmware_version,
            bluetooth_firmware_hash);
    env->DeleteLocalRef(product);
    env->DeleteLocalRef(firmware_version);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(securechip_model);
    env->DeleteLocalRef(bluetooth_firmware_version);
    env->DeleteLocalRef(bluetooth_firmware_hash);
    env->DeleteLocalRef(device_class);
    return result;
}

jobject toInitializeResult(
        JNIEnv *env,
        const nunchuk::bitbox::InitializeResult &initialize_result) {
    auto result_class = env->FindClass("com/nunchuk/android/bitbox/BitBoxInitializeResult");
    auto constructor = env->GetMethodID(
            result_class,
            "<init>",
            "(Lcom/nunchuk/android/bitbox/BitBoxDeviceInfo;Lcom/nunchuk/android/bitbox/BitBoxAttestationStatus;Ljava/lang/String;)V");
    auto device = toBitBoxDeviceInfo(env, initialize_result.device);
    auto attestation = enumValue(
            env,
            "com/nunchuk/android/bitbox/BitBoxAttestationStatus",
            attestationOrdinal(initialize_result.attestation));
    auto message = env->NewStringUTF(initialize_result.attestation_message.c_str());
    auto result = env->NewObject(result_class, constructor, device, attestation, message);
    env->DeleteLocalRef(device);
    env->DeleteLocalRef(attestation);
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(result_class);
    return result;
}

jobject toBitBoxBackups(
        JNIEnv *env,
        const std::vector<nunchuk::bitbox::BitBoxBackup> &backups) {
    auto list_class = env->FindClass("java/util/ArrayList");
    auto list_constructor = env->GetMethodID(list_class, "<init>", "()V");
    auto add_method = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    auto backup_class = env->FindClass("com/nunchuk/android/bitbox/BitBoxBackup");
    auto backup_constructor = env->GetMethodID(
            backup_class,
            "<init>",
            "(Ljava/lang/String;Ljava/lang/String;J)V");
    auto result = env->NewObject(list_class, list_constructor);
    for (const auto &backup: backups) {
        auto id = env->NewStringUTF(backup.id.c_str());
        auto name = env->NewStringUTF(backup.name.c_str());
        auto item = env->NewObject(
                backup_class,
                backup_constructor,
                id,
                name,
                static_cast<jlong>(backup.timestamp));
        env->CallBooleanMethod(result, add_method, item);
        env->DeleteLocalRef(item);
        env->DeleteLocalRef(name);
        env->DeleteLocalRef(id);
    }
    env->DeleteLocalRef(backup_class);
    env->DeleteLocalRef(list_class);
    return result;
}

nunchuk::bitbox::BitBoxTransport toBitBoxTransport(JNIEnv *env, jobject transport) {
    return enumOrdinal(env, transport) == 0
           ? nunchuk::bitbox::BitBoxTransport::BLE
           : nunchuk::bitbox::BitBoxTransport::USB_HID;
}

}  // namespace

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxCreateSession(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject transport) {
    try {
        auto &session = manager().forSession(
                toString(env, session_id),
                toBitBoxTransport(env, transport));
        return toBitBoxStep(env, session.initialize());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxConfirmPairing(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jboolean accepted) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.confirmPairing(accepted == JNI_TRUE));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxSetDeviceName(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring name) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.setDeviceName(toString(env, name)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxChangePassword(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.changePassword());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxSetMnemonicPassphraseEnabled(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jboolean enabled) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.setMnemonicPassphraseEnabled(enabled == JNI_TRUE));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxCreateNewSeed(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject mnemonic_length) {
    try {
        const auto length = enumOrdinal(env, mnemonic_length) == 0
                            ? nunchuk::bitbox::BitBoxMnemonicLength::WORDS_12
                            : nunchuk::bitbox::BitBoxMnemonicLength::WORDS_24;
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.createNewSeed(length));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxShowMnemonic(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.showMnemonic());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxCheckSdCard(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.checkSdCard());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxInsertSdCard(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.insertSdCard());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxCreateBackup(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.createBackup());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxCheckBackup(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jboolean silent) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.checkBackup(silent == JNI_TRUE));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxListBackups(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.listBackups());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxRestoreBackup(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.restoreBackup(toString(env, id)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxRestoreFromMnemonic(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.restoreFromMnemonic());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetExtendedPublicKey(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path,
        jboolean check_on_device) {
    try {
        nunchuk::bitbox::GetExtendedPublicKeyOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.getExtendedPublicKey(toString(env, derivation_path), options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetMasterFingerprint(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.getMasterFingerprint());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxIsWalletRegistered(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.isWalletRegistered(
                Serializer::convert2CWallet(env, wallet)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxRegisterWallet(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.registerWallet(
                Serializer::convert2CWallet(env, wallet)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxRegisterWalletContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.registerWallet(
                        parseWalletContent(env, wallet_content, wallet_name)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetWalletAddress(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet,
        jint address_index,
        jboolean check_on_device,
        jboolean change) {
    try {
        if (address_index < 0) {
            throw std::invalid_argument("Address index must be non-negative");
        }
        nunchuk::bitbox::WalletAddressOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        options.change = change == JNI_TRUE;
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.getWalletAddress(
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
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetWalletAddressContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name,
        jint address_index,
        jboolean check_on_device,
        jboolean change) {
    try {
        if (address_index < 0) {
            throw std::invalid_argument("Address index must be non-negative");
        }
        nunchuk::bitbox::WalletAddressOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        options.change = change == JNI_TRUE;
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.getWalletAddress(
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
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxSignMessage(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path,
        jstring message) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.signMessage(
                        toString(env, derivation_path),
                        toString(env, message)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxSignPsbt(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject wallet,
        jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.signPsbt(
                        Serializer::convert2CWallet(env, wallet),
                        toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxSignPsbtContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name,
        jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(
                env,
                session.signPsbt(
                        parseWalletContent(env, wallet_content, wallet_name),
                        toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxResume(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toBitBoxStep(env, session.resume());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxOnData(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jbyteArray data) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        auto bytes = toBytes(env, data);
        return toBitBoxStep(env, session.onData(bytes));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxIsInitialized(
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
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetDeviceInfo(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto device = manager().forSession(toString(env, session_id)).deviceInfo();
        return toBitBoxDeviceInfo(env, device);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetInitializeResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::InitializeResult>();
        return toInitializeResult(env, result);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetSdCardInsertedResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::SdCardStatusResult>();
        return static_cast<jboolean>(result.inserted);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return JNI_FALSE;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetBackupsResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::ListBackupsResult>();
        return toBitBoxBackups(env, result.backups);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetCheckedBackupIdResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::CheckBackupResult>();
        return env->NewStringUTF(result.backup_id.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetExtendedPublicKeyResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::GetExtendedPublicKeyResult>();
        return env->NewStringUTF(result.extended_public_key.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetMasterFingerprintResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::GetMasterFingerprintResult>();
        return env->NewStringUTF(result.master_fingerprint.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetRegistrationResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::RegistrationResult>();
        return static_cast<jboolean>(result.registered);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return JNI_FALSE;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetWalletAddressResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::WalletAddressResult>();
        return env->NewStringUTF(result.address.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetMessageSignatureResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::SignMessageResult>();
        return env->NewStringUTF(result.signature.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_bitBoxGetSignPsbtResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto result = manager()
                .forSession(toString(env, session_id))
                .result<nunchuk::bitbox::SignPsbtResult>();
        return env->NewStringUTF(result.psbt.c_str());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}
