#include <jni.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nunchuk.h>

#include "deserializer.h"
#include "serializer.h"
#include "string-wrapper.h"
#include "utils/ledger/ledger_manager.hpp"
#include "utils/ledger/types.hpp"

namespace {

nunchuk::ledger::LedgerManager g_ledger_manager;

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

std::vector<std::string> toStringVector(JNIEnv *env, jobject list_object) {
    std::vector<std::string> values;
    auto list_class = env->GetObjectClass(list_object);
    auto size_method = env->GetMethodID(list_class, "size", "()I");
    auto get_method = env->GetMethodID(list_class, "get", "(I)Ljava/lang/Object;");
    const auto size = env->CallIntMethod(list_object, size_method);
    values.reserve(static_cast<size_t>(size));
    for (jint i = 0; i < size; ++i) {
        auto value_object = static_cast<jstring>(env->CallObjectMethod(list_object, get_method, i));
        values.push_back(toString(env, value_object));
        env->DeleteLocalRef(value_object);
    }
    env->DeleteLocalRef(list_class);
    return values;
}

jobject toWriteList(JNIEnv *env, const std::vector<std::vector<unsigned char>> &writes) {
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

jobject toLedgerError(JNIEnv *env, const std::optional<nunchuk::ledger::LedgerError> &error) {
    if (!error.has_value()) {
        return nullptr;
    }

    auto error_class = env->FindClass("com/nunchuk/android/ledger/LedgerError");
    auto constructor = env->GetMethodID(error_class, "<init>", "(ILjava/lang/String;I)V");
    auto message = env->NewStringUTF(error->message.c_str());
    auto result = env->NewObject(
            error_class,
            constructor,
            static_cast<jint>(error->code),
            message,
            static_cast<jint>(error->status_word));
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(error_class);
    return result;
}

int stepOrdinal(nunchuk::ledger::LedgerStepType type) {
    switch (type) {
        case nunchuk::ledger::LedgerStepType::WRITE:
            return 0;
        case nunchuk::ledger::LedgerStepType::READ_MORE:
            return 1;
        case nunchuk::ledger::LedgerStepType::COMPLETE:
            return 2;
        case nunchuk::ledger::LedgerStepType::FAILED:
            return 3;
        case nunchuk::ledger::LedgerStepType::APP_SWITCH:
            return 4;
    }
    return 3;
}

int interactionOrdinal(nunchuk::ledger::UserInteraction interaction) {
    switch (interaction) {
        case nunchuk::ledger::UserInteraction::NONE:
            return 0;
        case nunchuk::ledger::UserInteraction::UNLOCK_DEVICE:
            return 1;
        case nunchuk::ledger::UserInteraction::CONFIRM_OPEN_APP:
            return 2;
        case nunchuk::ledger::UserInteraction::VERIFY_ADDRESS:
            return 3;
        case nunchuk::ledger::UserInteraction::REGISTER_WALLET:
            return 4;
        case nunchuk::ledger::UserInteraction::SIGN_MESSAGE:
            return 5;
        case nunchuk::ledger::UserInteraction::SIGN_TRANSACTION:
            return 6;
    }
    return 0;
}

jobject toLedgerStep(JNIEnv *env, const nunchuk::ledger::LedgerStep &step) {
    auto step_class = env->FindClass("com/nunchuk/android/ledger/LedgerStep");
    auto constructor = env->GetMethodID(
            step_class,
            "<init>",
            "(Lcom/nunchuk/android/ledger/LedgerStepType;Lcom/nunchuk/android/ledger/UserInteraction;Ljava/util/List;Lcom/nunchuk/android/ledger/LedgerError;)V");
    auto type = enumValue(env, "com/nunchuk/android/ledger/LedgerStepType", stepOrdinal(step.type));
    auto interaction = enumValue(
            env,
            "com/nunchuk/android/ledger/UserInteraction",
            interactionOrdinal(step.interaction));
    auto writes = toWriteList(env, step.writes);
    auto error = toLedgerError(env, step.error);
    auto result = env->NewObject(step_class, constructor, type, interaction, writes, error);

    env->DeleteLocalRef(type);
    env->DeleteLocalRef(interaction);
    env->DeleteLocalRef(writes);
    if (error) {
        env->DeleteLocalRef(error);
    }
    env->DeleteLocalRef(step_class);
    return result;
}

jobject toLedgerRegisteredWalletResult(
        JNIEnv *env,
        const nunchuk::ledger::RegisteredWalletResult &wallet) {
    auto result_class = env->FindClass("com/nunchuk/android/ledger/LedgerRegisteredWalletResult");
    auto constructor = env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;)V");
    auto hmac = env->NewStringUTF(wallet.hmac.c_str());
    auto result = env->NewObject(result_class, constructor, hmac);
    env->DeleteLocalRef(hmac);
    env->DeleteLocalRef(result_class);
    return result;
}

nunchuk::ledger::LedgerTransport toLedgerTransport(JNIEnv *env, jobject transport_object) {
    const auto transport_ordinal = enumOrdinal(env, transport_object);
    return transport_ordinal == 0
           ? nunchuk::ledger::LedgerTransport::BLE
           : nunchuk::ledger::LedgerTransport::USB_HID;
}

nunchuk::Bip388Policy toBip388Policy(JNIEnv *env, jobject policy_object) {
    auto policy_class = env->GetObjectClass(policy_object);
    auto descriptor_field = env->GetFieldID(policy_class, "descriptorTemplate", "Ljava/lang/String;");
    auto keys_field = env->GetFieldID(policy_class, "keysInfo", "Ljava/util/List;");

    auto descriptor_object = static_cast<jstring>(env->GetObjectField(policy_object, descriptor_field));
    auto keys_object = env->GetObjectField(policy_object, keys_field);

    nunchuk::Bip388Policy policy;
    policy.descriptor_template = toString(env, descriptor_object);
    policy.keys_info = toStringVector(env, keys_object);

    env->DeleteLocalRef(descriptor_object);
    env->DeleteLocalRef(keys_object);
    env->DeleteLocalRef(policy_class);
    return policy;
}

std::string walletPolicyName(JNIEnv *env, jobject policy_object) {
    auto policy_class = env->GetObjectClass(policy_object);
    auto name_field = env->GetFieldID(policy_class, "name", "Ljava/lang/String;");
    auto name_object = static_cast<jstring>(env->GetObjectField(policy_object, name_field));
    auto name = toString(env, name_object);
    env->DeleteLocalRef(name_object);
    env->DeleteLocalRef(policy_class);
    return name;
}

nunchuk::ledger::RegisteredWallet toRegisteredWallet(JNIEnv *env, jobject wallet_object) {
    auto wallet_class = env->GetObjectClass(wallet_object);
    auto name_field = env->GetFieldID(wallet_class, "name", "Ljava/lang/String;");
    auto descriptor_field = env->GetFieldID(wallet_class, "descriptorTemplate", "Ljava/lang/String;");
    auto keys_field = env->GetFieldID(wallet_class, "keysInfo", "Ljava/util/List;");
    auto hmac_field = env->GetFieldID(wallet_class, "hmac", "Ljava/lang/String;");

    auto name_object = static_cast<jstring>(env->GetObjectField(wallet_object, name_field));
    auto descriptor_object = static_cast<jstring>(env->GetObjectField(wallet_object, descriptor_field));
    auto keys_object = env->GetObjectField(wallet_object, keys_field);
    auto hmac_object = static_cast<jstring>(env->GetObjectField(wallet_object, hmac_field));

    nunchuk::ledger::RegisteredWallet wallet;
    wallet.name = toString(env, name_object);
    wallet.policy.descriptor_template = toString(env, descriptor_object);
    wallet.hmac = toString(env, hmac_object);
    wallet.policy.keys_info = toStringVector(env, keys_object);

    env->DeleteLocalRef(name_object);
    env->DeleteLocalRef(descriptor_object);
    env->DeleteLocalRef(keys_object);
    env->DeleteLocalRef(hmac_object);
    env->DeleteLocalRef(wallet_class);
    return wallet;
}

}  // namespace

extern "C"
JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerSetChain(
        JNIEnv *env,
        jobject thiz,
        jint chain) {
    try {
        nunchuk::Utils::SetChain(Serializer::convert2CChain(chain));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerCreateSession(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject transport) {
    try {
        g_ledger_manager.forSession(toString(env, session_id), toLedgerTransport(env, transport));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetExtendedPublicKey(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path,
        jboolean check_on_device) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        nunchuk::ledger::GetExtendedPublicKeyOptions options;
        options.check_on_device = check_on_device;
        auto step = session.getExtendedPublicKey(
                toString(env, derivation_path),
                options);
        return toLedgerStep(env, step);
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerResume(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(env, session.resume());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetMasterFingerprint(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(env, session.getMasterFingerprint());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerSignMessage(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring derivation_path,
        jstring message) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(env, session.signMessage(
                toString(env, derivation_path),
                toString(env, message)));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerRegisterWallet(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject policy) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto wallet_name = walletPolicyName(env, policy);
        auto bip388_policy = toBip388Policy(env, policy);
        return toLedgerStep(env, session.registerWallet(bip388_policy, wallet_name));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerRegisterWalletContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(
                env,
                session.registerWallet(
                        parseWalletContent(env, wallet_content, wallet_name)));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetWalletAddress(
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
        nunchuk::ledger::WalletAddressOptions options;
        options.check_on_device = check_on_device;
        options.change = change;
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(env, session.getWalletAddress(
                toRegisteredWallet(env, wallet),
                static_cast<uint32_t>(address_index),
                options));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetWalletAddressContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring wallet_content,
        jstring wallet_name,
        jstring wallet_hmac,
        jint address_index,
        jboolean check_on_device,
        jboolean change) {
    try {
        if (address_index < 0) {
            throw std::invalid_argument("Address index must be non-negative");
        }
        nunchuk::ledger::WalletAddressOptions options;
        options.check_on_device = check_on_device == JNI_TRUE;
        options.change = change == JNI_TRUE;
        auto wallet = parseWalletContent(env, wallet_content, wallet_name);
        auto registered_wallet = nunchuk::ledger::RegisteredWallet(
                wallet,
                toString(env, wallet_hmac));
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(
                env,
                session.getWalletAddress(
                        registered_wallet,
                        static_cast<uint32_t>(address_index),
                        options));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerSignPsbt(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring psbt,
        jobject wallet) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(env, session.signPsbt(
                toRegisteredWallet(env, wallet),
                toString(env, psbt)));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerSignPsbtContent(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jstring psbt,
        jstring wallet_content,
        jstring wallet_name,
        jstring wallet_hmac) {
    try {
        auto wallet = parseWalletContent(env, wallet_content, wallet_name);
        auto registered_wallet = nunchuk::ledger::RegisteredWallet(
                wallet,
                toString(env, wallet_hmac));
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        return toLedgerStep(
                env,
                session.signPsbt(registered_wallet, toString(env, psbt)));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerOnData(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jbyteArray data) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto step = session.onData(toBytes(env, data));
        return toLedgerStep(env, step);
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetMasterFingerprintStringResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::GetMasterFingerprintResult>();
        return env->NewStringUTF(result.master_fingerprint.c_str());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetMessageSignatureStringResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::SignMessageResult>();
        return env->NewStringUTF(result.signature.c_str());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetRegisteredWalletResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::RegisteredWalletResult>();
        return toLedgerRegisteredWalletResult(env, result);
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetExtendedPublicKeyResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::GetExtendedPublicKeyResult>();
        return env->NewStringUTF(result.extended_public_key.c_str());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetWalletAddressResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::WalletAddressResult>();
        return env->NewStringUTF(result.address.c_str());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerGetSignPsbtStringResult(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        auto &session = g_ledger_manager.forSession(toString(env, session_id));
        auto result = session.result<nunchuk::ledger::SignPsbtResult>();
        return env->NewStringUTF(result.psbt.c_str());
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}
