#include <jni.h>
#include <nunchuk.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "deserializer.h"
#include "nunchukprovider.h"
#include "serializer.h"
#include "string-wrapper.h"

using namespace nunchuk;

namespace {

void ThrowIfJavaException(JNIEnv *env, const std::string &context) {
    jthrowable exception = env->ExceptionOccurred();
    if (exception == nullptr) return;

    env->ExceptionClear();
    std::string message = "unknown Java exception";
    jclass exceptionClass = env->GetObjectClass(exception);
    if (exceptionClass != nullptr) {
        jmethodID toStringMethod = env->GetMethodID(exceptionClass, "toString", "()Ljava/lang/String;");
        if (toStringMethod != nullptr) {
            auto value = static_cast<jstring>(env->CallObjectMethod(exception, toStringMethod));
            if (!env->ExceptionCheck() && value != nullptr) {
                message = StringWrapper(env, value);
                env->DeleteLocalRef(value);
            } else {
                env->ExceptionClear();
            }
        } else {
            env->ExceptionClear();
        }
        env->DeleteLocalRef(exceptionClass);
    } else {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(exception);
    throw std::runtime_error(context + ": " + message);
}

jmethodID GetMethod(JNIEnv *env, jclass clazz, const char *name, const char *signature) {
    jmethodID method = env->GetMethodID(clazz, name, signature);
    ThrowIfJavaException(env, std::string("Satochip callback method lookup failed for ") + name);
    if (method == nullptr) {
        throw std::runtime_error(std::string("Satochip callback method not found: ") + name);
    }
    return method;
}

std::vector<unsigned char> ToBytes(JNIEnv *env, jbyteArray byteArray) {
    if (byteArray == nullptr) {
        throw std::runtime_error("Satochip callback returned null byte array.");
    }

    jsize length = env->GetArrayLength(byteArray);
    ThrowIfJavaException(env, "Failed to read Satochip byte array length");

    std::vector<unsigned char> result(static_cast<size_t>(length));
    if (length > 0) {
        env->GetByteArrayRegion(byteArray, 0, length, reinterpret_cast<jbyte *>(result.data()));
        ThrowIfJavaException(env, "Failed to read Satochip byte array");
    }
    return result;
}

jbyteArray ToJByteArray(JNIEnv *env, const std::vector<unsigned char> &bytes) {
    auto result = env->NewByteArray(static_cast<jsize>(bytes.size()));
    ThrowIfJavaException(env, "Failed to allocate Satochip byte array");
    if (!bytes.empty()) {
        env->SetByteArrayRegion(
                result,
                0,
                static_cast<jsize>(bytes.size()),
                reinterpret_cast<const jbyte *>(bytes.data())
        );
        ThrowIfJavaException(env, "Failed to populate Satochip byte array");
    }
    return result;
}

jbyteArray ToNullableJByteArray(
        JNIEnv *env,
        const std::optional<std::vector<unsigned char>> &bytes
) {
    if (!bytes.has_value()) return nullptr;
    return ToJByteArray(env, bytes.value());
}

std::optional<std::vector<unsigned char>> ToOptionalBytes(JNIEnv *env, jbyteArray byteArray) {
    if (byteArray == nullptr) return std::nullopt;
    return ToBytes(env, byteArray);
}

std::vector<std::vector<unsigned char>> ToBytesArray(JNIEnv *env, jobjectArray arrays) {
    if (arrays == nullptr) {
        throw std::runtime_error("Satochip callback returned null byte array list.");
    }

    jsize length = env->GetArrayLength(arrays);
    ThrowIfJavaException(env, "Failed to read Satochip byte array list length");

    std::vector<std::vector<unsigned char>> result;
    result.reserve(static_cast<size_t>(length));
    for (jsize i = 0; i < length; ++i) {
        auto item = static_cast<jbyteArray>(env->GetObjectArrayElement(arrays, i));
        ThrowIfJavaException(env, "Failed to read Satochip byte array list item");
        result.push_back(ToBytes(env, item));
        env->DeleteLocalRef(item);
    }
    return result;
}

class SatochipCallbackAdapter {
public:
    SatochipCallbackAdapter(JNIEnv *env, jobject callback)
            : env_(env), callback_(callback) {
        if (callback_ == nullptr) {
            throw std::runtime_error("Satochip callback is required.");
        }

        callbackClass_ = env_->GetObjectClass(callback_);
        ThrowIfJavaException(env_, "Failed to inspect Satochip callback");

        cardBip32GetExtendedKeyMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardBip32GetExtendedKey",
                "(Ljava/lang/String;)[[B"
        );
        cardSignTransactionHashMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardSignTransactionHash",
                "(I[B[B)[B"
        );
        cardTaprootTweakPrivateKeyMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardTaprootTweakPrivateKey",
                "(I[BZ)[B"
        );
        cardSignSchnorrHashMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardSignSchnorrHash",
                "([B[B)[B"
        );
        cardMusig2GenerateNonceMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardMusig2GenerateNonce",
                "(I[B[B[B)[[B"
        );
        cardMusig2SignMethod_ = GetMethod(
                env_,
                callbackClass_,
                "cardMusig2Sign",
                "(I[B[B[BZZ)[B"
        );
    }

    ~SatochipCallbackAdapter() {
        if (callbackClass_ != nullptr) {
            env_->DeleteLocalRef(callbackClass_);
        }
    }

    std::vector<std::vector<unsigned char>> CardBip32GetExtendedKey(
            const std::string &path
    ) {
        jstring pathString = env_->NewStringUTF(path.c_str());
        ThrowIfJavaException(env_, "Failed to allocate Satochip BIP32 path");

        auto response = static_cast<jobjectArray>(env_->CallObjectMethod(
                callback_,
                cardBip32GetExtendedKeyMethod_,
                pathString
        ));
        env_->DeleteLocalRef(pathString);
        ThrowIfJavaException(env_, "Satochip cardBip32GetExtendedKey callback failed");

        auto result = ToBytesArray(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

    std::vector<unsigned char> CardSignTransactionHash(
            int keyNumber,
            const std::vector<unsigned char> &txHash,
            const std::optional<std::vector<unsigned char>> &challengeResponse
    ) {
        jbyteArray txHashArray = ToJByteArray(env_, txHash);
        jbyteArray challengeResponseArray = ToNullableJByteArray(env_, challengeResponse);

        auto response = static_cast<jbyteArray>(env_->CallObjectMethod(
                callback_,
                cardSignTransactionHashMethod_,
                static_cast<jint>(keyNumber),
                txHashArray,
                challengeResponseArray
        ));
        env_->DeleteLocalRef(txHashArray);
        if (challengeResponseArray != nullptr) env_->DeleteLocalRef(challengeResponseArray);
        ThrowIfJavaException(env_, "Satochip cardSignTransactionHash callback failed");

        auto result = ToBytes(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

    std::vector<unsigned char> CardTaprootTweakPrivateKey(
            int keyNumber,
            const std::vector<unsigned char> &tweak,
            bool bypassFlag
    ) {
        jbyteArray tweakArray = ToJByteArray(env_, tweak);

        auto response = static_cast<jbyteArray>(env_->CallObjectMethod(
                callback_,
                cardTaprootTweakPrivateKeyMethod_,
                static_cast<jint>(keyNumber),
                tweakArray,
                static_cast<jboolean>(bypassFlag)
        ));
        env_->DeleteLocalRef(tweakArray);
        ThrowIfJavaException(env_, "Satochip cardTaprootTweakPrivateKey callback failed");

        auto result = ToBytes(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

    std::vector<unsigned char> CardSignSchnorrHash(
            const std::vector<unsigned char> &txHash,
            const std::optional<std::vector<unsigned char>> &challengeResponse
    ) {
        jbyteArray txHashArray = ToJByteArray(env_, txHash);
        jbyteArray challengeResponseArray = ToNullableJByteArray(env_, challengeResponse);

        auto response = static_cast<jbyteArray>(env_->CallObjectMethod(
                callback_,
                cardSignSchnorrHashMethod_,
                txHashArray,
                challengeResponseArray
        ));
        env_->DeleteLocalRef(txHashArray);
        if (challengeResponseArray != nullptr) env_->DeleteLocalRef(challengeResponseArray);
        ThrowIfJavaException(env_, "Satochip cardSignSchnorrHash callback failed");

        auto result = ToBytes(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

    std::vector<std::vector<unsigned char>> CardMusig2GenerateNonce(
            int keyNumber,
            const std::vector<unsigned char> &aggregatePublicKey,
            const std::vector<unsigned char> &message,
            const std::vector<unsigned char> &extra
    ) {
        jbyteArray aggregatePublicKeyArray = ToJByteArray(env_, aggregatePublicKey);
        jbyteArray messageArray = ToJByteArray(env_, message);
        jbyteArray extraArray = ToJByteArray(env_, extra);

        auto response = static_cast<jobjectArray>(env_->CallObjectMethod(
                callback_,
                cardMusig2GenerateNonceMethod_,
                static_cast<jint>(keyNumber),
                aggregatePublicKeyArray,
                messageArray,
                extraArray
        ));
        env_->DeleteLocalRef(aggregatePublicKeyArray);
        env_->DeleteLocalRef(messageArray);
        env_->DeleteLocalRef(extraArray);
        ThrowIfJavaException(env_, "Satochip cardMusig2GenerateNonce callback failed");

        auto result = ToBytesArray(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

    std::vector<unsigned char> CardMusig2Sign(
            int keyNumber,
            const std::vector<unsigned char> &secNonce,
            const std::vector<unsigned char> &b,
            const std::vector<unsigned char> &ea,
            bool rHasEvenY,
            bool ggaccIsOne
    ) {
        jbyteArray secNonceArray = ToJByteArray(env_, secNonce);
        jbyteArray bArray = ToJByteArray(env_, b);
        jbyteArray eaArray = ToJByteArray(env_, ea);

        auto response = static_cast<jbyteArray>(env_->CallObjectMethod(
                callback_,
                cardMusig2SignMethod_,
                static_cast<jint>(keyNumber),
                secNonceArray,
                bArray,
                eaArray,
                static_cast<jboolean>(rHasEvenY),
                static_cast<jboolean>(ggaccIsOne)
        ));
        env_->DeleteLocalRef(secNonceArray);
        env_->DeleteLocalRef(bArray);
        env_->DeleteLocalRef(eaArray);
        ThrowIfJavaException(env_, "Satochip cardMusig2Sign callback failed");

        auto result = ToBytes(env_, response);
        env_->DeleteLocalRef(response);
        return result;
    }

private:
    JNIEnv *env_;
    jobject callback_;
    jclass callbackClass_{};
    jmethodID cardBip32GetExtendedKeyMethod_{};
    jmethodID cardSignTransactionHashMethod_{};
    jmethodID cardTaprootTweakPrivateKeyMethod_{};
    jmethodID cardSignSchnorrHashMethod_{};
    jmethodID cardMusig2GenerateNonceMethod_{};
    jmethodID cardMusig2SignMethod_{};
};

} // namespace

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_getSatochipSigner(
        JNIEnv *env,
        jobject thiz,
        jobject callback,
        jstring path
) {
    try {
        SatochipCallbackAdapter adapter(env, callback);
        auto signer = NunchukProvider::get()->nu->GetSatochipSigner(
                [&](const std::string &requestedPath) {
                    return adapter.CardBip32GetExtendedKey(requestedPath);
                },
                StringWrapper(env, path)
        );
        return Deserializer::convert2JSigner(env, signer);
    } catch (BaseException &e) {
        Deserializer::convert2JException(env, e);
        return nullptr;
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_signSatochipTransaction(
        JNIEnv *env,
        jobject thiz,
        jobject callback,
        jstring wallet_id,
        jstring tx_id,
        jbyteArray challenge_response
) {
    try {
        SatochipCallbackAdapter adapter(env, callback);

        SatochipSignPsbtParams params;
        params.chalresponse = ToOptionalBytes(env, challenge_response);
        params.cardBip32GetExtendedKeyFn = [&](const std::string &path) {
            return adapter.CardBip32GetExtendedKey(path);
        };
        params.cardSignTransactionHashFn = [&](unsigned char keyNumber,
                                               const std::vector<unsigned char> &txHash,
                                               const std::optional<std::vector<unsigned char>> &chalresponse) {
            return adapter.CardSignTransactionHash(static_cast<int>(keyNumber), txHash, chalresponse);
        };
        params.cardTaprootTweakPrivateKeyFn = [&](int keyNumber,
                                                  const std::vector<unsigned char> &tweak,
                                                  bool bypassFlag) {
            return adapter.CardTaprootTweakPrivateKey(keyNumber, tweak, bypassFlag);
        };
        params.cardSignSchnorrHashFn = [&](const std::vector<unsigned char> &txHash,
                                           const std::optional<std::vector<unsigned char>> &chalresponse) {
            return adapter.CardSignSchnorrHash(txHash, chalresponse);
        };
        params.cardMusig2GenerateNonceFn = [&](int keyNumber,
                                               const std::vector<unsigned char> &aggregatePublicKey,
                                               const std::vector<unsigned char> &message,
                                               const std::vector<unsigned char> &extra) {
            return adapter.CardMusig2GenerateNonce(keyNumber, aggregatePublicKey, message, extra);
        };
        params.cardMusig2SignFn = [&](int keyNumber,
                                      const std::vector<unsigned char> &secNonce,
                                      const std::vector<unsigned char> &b,
                                      const std::vector<unsigned char> &ea,
                                      bool rHasEvenY,
                                      bool ggaccIsOne) {
            return adapter.CardMusig2Sign(keyNumber, secNonce, b, ea, rHasEvenY, ggaccIsOne);
        };

        auto transaction = NunchukProvider::get()->nu->SignSatochipTransaction(
                params,
                StringWrapper(env, wallet_id),
                StringWrapper(env, tx_id)
        );
        return Deserializer::convert2JTransaction(env, transaction);
    } catch (BaseException &e) {
        Deserializer::convert2JException(env, e);
        return nullptr;
    } catch (std::exception &e) {
        Deserializer::convertStdException2JException(env, e);
        return nullptr;
    }
}
