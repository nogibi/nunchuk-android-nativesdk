#include <jni.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "deserializer.h"
#include "string-wrapper.h"
#include "utils/ledger/ledger_manager.hpp"
#include "utils/ledger/types.hpp"

namespace {

nunchuk::ledger::LedgerManager g_ledger_manager;

std::string toString(JNIEnv *env, jstring value) {
    return StringWrapper(env, value);
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

jobject toLedgerStep(JNIEnv *env, const nunchuk::ledger::LedgerStep &step) {
    auto step_class = env->FindClass("com/nunchuk/android/ledger/LedgerStep");
    auto constructor = env->GetMethodID(
            step_class,
            "<init>",
            "(Lcom/nunchuk/android/ledger/LedgerStepType;Ljava/util/List;Lcom/nunchuk/android/ledger/LedgerError;)V");
    auto type = enumValue(env, "com/nunchuk/android/ledger/LedgerStepType", stepOrdinal(step.type));
    auto writes = toWriteList(env, step.writes);
    auto error = toLedgerError(env, step.error);
    auto result = env->NewObject(step_class, constructor, type, writes, error);

    env->DeleteLocalRef(type);
    env->DeleteLocalRef(writes);
    if (error) {
        env->DeleteLocalRef(error);
    }
    env->DeleteLocalRef(step_class);
    return result;
}

nunchuk::ledger::LedgerSessionConfig toLedgerSessionConfig(JNIEnv *env, jobject config_object) {
    auto config_class = env->GetObjectClass(config_object);
    auto transport_field = env->GetFieldID(
            config_class,
            "transport",
            "Lcom/nunchuk/android/ledger/LedgerTransport;");
    auto packet_size_field = env->GetFieldID(config_class, "packetSize", "I");
    auto channel_field = env->GetFieldID(config_class, "channel", "I");

    auto transport_object = env->GetObjectField(config_object, transport_field);
    const auto transport_ordinal = enumOrdinal(env, transport_object);

    nunchuk::ledger::LedgerSessionConfig config;
    config.transport = transport_ordinal == 0
                       ? nunchuk::ledger::LedgerTransport::BLE
                       : nunchuk::ledger::LedgerTransport::USB_HID;
    config.packet_size = static_cast<size_t>(env->GetIntField(config_object, packet_size_field));
    config.channel = static_cast<uint16_t>(env->GetIntField(config_object, channel_field));

    env->DeleteLocalRef(transport_object);
    env->DeleteLocalRef(config_class);
    return config;
}

nunchuk::ledger::GetExtendedPublicKeyOptions getXpubOptions(jboolean check_on_device) {
    nunchuk::ledger::GetExtendedPublicKeyOptions options;
    options.check_on_device = check_on_device;
    return options;
}

}  // namespace

extern "C"
JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerCreateSession(
        JNIEnv *env,
        jobject thiz,
        jstring session_id,
        jobject config) {
    try {
        g_ledger_manager.createSession(toString(env, session_id), toLedgerSessionConfig(env, config));
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_ledgerReset(
        JNIEnv *env,
        jobject thiz,
        jstring session_id) {
    try {
        g_ledger_manager.forSession(toString(env, session_id)).reset();
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
        auto step = session.getExtendedPublicKey(
                toString(env, derivation_path),
                getXpubOptions(check_on_device));
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
