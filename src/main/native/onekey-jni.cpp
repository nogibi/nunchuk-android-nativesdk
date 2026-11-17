#include <jni.h>

#include <algorithm>
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
#include "utils/onekey/onekey.hpp"

namespace {

nunchuk::onekey::OneKeyManager &manager() {
    auto *provider = NunchukProvider::get();
    if (!provider->nu || !provider->oneKeyManager) {
        throw std::runtime_error("Nunchuk must be initialized before using OneKey");
    }
    return *provider->oneKeyManager;
}

std::string toString(JNIEnv *env, jstring value) { return StringWrapper(env, value); }

nunchuk::Wallet parseWalletContent(JNIEnv *env, jstring wallet_content, jstring wallet_name) {
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
        env->SetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()),
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

jobject toWriteList(JNIEnv *env, const std::vector<std::vector<unsigned char>> &writes) {
    auto list_class = env->FindClass("java/util/ArrayList");
    auto constructor = env->GetMethodID(list_class, "<init>", "()V");
    auto add_method = env->GetMethodID(list_class, "add", "(Ljava/lang/Object;)Z");
    auto list = env->NewObject(list_class, constructor);
    for (const auto &write : writes) {
        auto array = toByteArray(env, write);
        env->CallBooleanMethod(list, add_method, array);
        env->DeleteLocalRef(array);
    }
    env->DeleteLocalRef(list_class);
    return list;
}

// Use standard UTF-8 for messages/passphrases, rather than JNI modified UTF-8.
std::string toUtf8(JNIEnv *env, jbyteArray data) {
    auto bytes = toBytes(env, data);
    std::string result(bytes.begin(), bytes.end());
    std::fill(bytes.begin(), bytes.end(), 0);
    return result;
}

jstring toJString(JNIEnv *env, const std::string &text) {
    auto bytes = toByteArray(env, {text.begin(), text.end()});
    auto charset = env->NewStringUTF("UTF-8");
    auto string_class = env->FindClass("java/lang/String");
    auto constructor = env->GetMethodID(string_class, "<init>", "([BLjava/lang/String;)V");
    auto result = static_cast<jstring>(env->NewObject(string_class, constructor, bytes, charset));
    env->DeleteLocalRef(string_class);
    env->DeleteLocalRef(charset);
    env->DeleteLocalRef(bytes);
    return result;
}

jobject toOneKeyError(JNIEnv *env, const std::optional<nunchuk::onekey::OneKeyError> &error) {
    if (!error) return nullptr;
    auto clazz = env->FindClass("com/nunchuk/android/onekey/OneKeyError");
    auto constructor = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;I)V");
    auto message = toJString(env, error->message);
    auto result = env->NewObject(clazz, constructor, static_cast<jint>(error->code), message,
                                 static_cast<jint>(error->device_code));
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(clazz);
    return result;
}

jobject toOneKeyStep(JNIEnv *env, const nunchuk::onekey::OneKeyStep &step) {
    auto clazz = env->FindClass("com/nunchuk/android/onekey/OneKeyStep");
    auto constructor = env->GetMethodID(
        clazz, "<init>",
        "(Lcom/nunchuk/android/onekey/OneKeyStepType;Lcom/nunchuk/android/onekey/"
        "OneKeyUserInteraction;Ljava/util/List;Lcom/nunchuk/android/onekey/OneKeyError;IZZZ)V");
    auto type =
        enumValue(env, "com/nunchuk/android/onekey/OneKeyStepType", static_cast<int>(step.type));
    auto interaction = enumValue(env, "com/nunchuk/android/onekey/OneKeyUserInteraction",
                                 static_cast<int>(step.interaction));
    auto writes = toWriteList(env, step.writes);
    auto error = toOneKeyError(env, step.error);
    auto result = env->NewObject(clazz, constructor, type, interaction, writes, error,
                                 static_cast<jint>(step.request_code),
                                 static_cast<jboolean>(step.on_device_available),
                                 static_cast<jboolean>(step.on_device_required),
                                 static_cast<jboolean>(step.attach_pin_available));
    if (error) env->DeleteLocalRef(error);
    env->DeleteLocalRef(writes);
    env->DeleteLocalRef(interaction);
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(clazz);
    return result;
}

jobject toOneKeyDeviceInfo(JNIEnv *env, const nunchuk::onekey::OneKeyDeviceInfo &info) {
    auto clazz = env->FindClass("com/nunchuk/android/onekey/OneKeyDeviceInfo");
    auto constructor = env->GetMethodID(clazz, "<init>",
                                        "(Lcom/nunchuk/android/onekey/OneKeyModel;Ljava/lang/"
                                        "String;Ljava/lang/String;Ljava/lang/String;ZZZZZZZ)V");
    auto model =
        enumValue(env, "com/nunchuk/android/onekey/OneKeyModel", static_cast<int>(info.model));
    auto firmware = toJString(env, info.firmware_version);
    auto id = toJString(env, info.device_id);
    auto label = toJString(env, info.label);
    auto result = env->NewObject(
        clazz, constructor, model, firmware, id, label, static_cast<jboolean>(info.initialized),
        static_cast<jboolean>(info.unlocked), static_cast<jboolean>(info.pin_protection),
        static_cast<jboolean>(info.passphrase_protection),
        static_cast<jboolean>(info.passphrase_on_device), static_cast<jboolean>(info.bootloader),
        static_cast<jboolean>(info.supports_raw_psbt));
    env->DeleteLocalRef(label);
    env->DeleteLocalRef(id);
    env->DeleteLocalRef(firmware);
    env->DeleteLocalRef(model);
    env->DeleteLocalRef(clazz);
    return result;
}

}  // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyCreateSession(JNIEnv *env, jobject thiz,
                                                                         jstring session_id,
                                                                         jobject transport) {
    try {
        const auto ordinal = enumOrdinal(env, transport);
        if (ordinal < 0 || ordinal > 1) throw std::invalid_argument("Invalid OneKey transport");
        auto &session = manager().forSession(
            toString(env, session_id), static_cast<nunchuk::onekey::OneKeyTransport>(ordinal));
        return toOneKeyStep(env, session.initialize());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetMasterFingerprint(
    JNIEnv *env, jobject thiz, jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.getMasterFingerprint());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyEnterPinOnDevice(JNIEnv *env,
                                                                            jobject thiz,
                                                                            jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.enterPinOnDevice());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyCancel(JNIEnv *env, jobject thiz,
                                                                  jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.cancel());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyOnDisconnected(JNIEnv *env, jobject thiz,
                                                                          jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.onDisconnected());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetExtendedPublicKey(
    JNIEnv *env, jobject thiz, jstring session_id, jstring derivation_path,
    jboolean check_on_device) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        nunchuk::onekey::GetExtendedPublicKeyOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        return toOneKeyStep(env,
                            session.getExtendedPublicKey(toString(env, derivation_path), options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyProvidePin(JNIEnv *env, jobject thiz,
                                                                      jstring session_id,
                                                                      jstring pin) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.providePin(toString(env, pin)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyProvidePassphrase(
    JNIEnv *env, jobject thiz, jstring session_id, jbyteArray passphrase) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.providePassphrase(toUtf8(env, passphrase)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyEnterPassphraseOnDevice(
    JNIEnv *env, jobject thiz, jstring session_id, jboolean attach_to_pin) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.enterPassphraseOnDevice(attach_to_pin == JNI_TRUE));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeySignMessage(JNIEnv *env, jobject thiz,
                                                                       jstring session_id,
                                                                       jstring derivation_path,
                                                                       jbyteArray message) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(
            env, session.signMessage(toString(env, derivation_path), toUtf8(env, message)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyOnData(JNIEnv *env, jobject thiz,
                                                                  jstring session_id,
                                                                  jbyteArray data) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env, session.onData(toBytes(env, data)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetWalletAddress(
    JNIEnv *env, jobject thiz, jstring session_id, jobject wallet, jint address_index,
    jboolean check_on_device, jboolean change) {
    try {
        if (address_index < 0) throw std::invalid_argument("Address index must be non-negative");
        auto &session = manager().forSession(toString(env, session_id));
        nunchuk::onekey::WalletAddressOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        options.change = change == JNI_TRUE;
        return toOneKeyStep(
            env, session.getWalletAddress(Serializer::convert2CWallet(env, wallet),
                                          static_cast<uint32_t>(address_index), options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeySignPsbt(JNIEnv *env, jobject thiz,
                                                                    jstring session_id,
                                                                    jobject wallet, jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(
            env, session.signPsbt(Serializer::convert2CWallet(env, wallet), toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetWalletAddressContent(
    JNIEnv *env, jobject thiz, jstring session_id, jstring wallet_content, jstring wallet_name,
    jint address_index, jboolean check_on_device, jboolean change) {
    try {
        if (address_index < 0) throw std::invalid_argument("Address index must be non-negative");
        auto &session = manager().forSession(toString(env, session_id));
        nunchuk::onekey::WalletAddressOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        options.change = change == JNI_TRUE;
        return toOneKeyStep(
            env, session.getWalletAddress(parseWalletContent(env, wallet_content, wallet_name),
                                          static_cast<uint32_t>(address_index), options));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeySignPsbtContent(
    JNIEnv *env, jobject thiz, jstring session_id, jstring wallet_content, jstring wallet_name,
    jstring psbt) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyStep(env,
                            session.signPsbt(parseWalletContent(env, wallet_content, wallet_name),
                                             toString(env, psbt)));
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyIsInitialized(JNIEnv *env, jobject thiz,
                                                                         jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return static_cast<jboolean>(session.initialized());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetDeviceInfo(JNIEnv *env, jobject thiz,
                                                                         jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toOneKeyDeviceInfo(env, session.deviceInfo());
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetExtendedPublicKeyResult(
    JNIEnv *env, jobject thiz, jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJString(
            env, session.result<nunchuk::onekey::GetExtendedPublicKeyResult>().extended_public_key);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetMasterFingerprintResult(
    JNIEnv *env, jobject thiz, jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJString(
            env, session.result<nunchuk::onekey::GetMasterFingerprintResult>().master_fingerprint);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetWalletAddressResult(
    JNIEnv *env, jobject thiz, jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJString(env, session.result<nunchuk::onekey::WalletAddressResult>().address);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetMessageSignatureResult(
    JNIEnv *env, jobject thiz, jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJString(env, session.result<nunchuk::onekey::SignMessageResult>().signature);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_oneKeyGetSignPsbtResult(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jstring session_id) {
    try {
        auto &session = manager().forSession(toString(env, session_id));
        return toJString(env, session.result<nunchuk::onekey::SignPsbtResult>().psbt);
    } catch (const std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}
