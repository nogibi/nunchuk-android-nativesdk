#include <jni.h>
#include <nunchuk.h>
#include <openssl/crypto.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "deserializer.h"
#include "nunchukprovider.h"
#include "serializer.h"
#include "string-wrapper.h"
#include "utils/satochip.hpp"

namespace {
using Bytes = std::vector<unsigned char>;
struct JavaException {};

void checkJava(JNIEnv *env) {
    if (env->ExceptionCheck()) throw JavaException{};
}

class LocalFrame {
   public:
    explicit LocalFrame(JNIEnv *env) : env_(env) {
        if (env_->PushLocalFrame(32) < 0) throw JavaException{};
    }
    ~LocalFrame() { env_->PopLocalFrame(nullptr); }

   private:
    JNIEnv *env_;
};

Bytes bytes(JNIEnv *env, jbyteArray value) {
    checkJava(env);
    if (!value) throw std::runtime_error("Missing Satochip callback result");
    Bytes result(env->GetArrayLength(value));
    if (!result.empty()) {
        env->GetByteArrayRegion(value, 0, result.size(), reinterpret_cast<jbyte *>(result.data()));
        checkJava(env);
    }
    return result;
}

jbyteArray array(JNIEnv *env, const Bytes &value) {
    auto result = env->NewByteArray(value.size());
    checkJava(env);
    if (!value.empty()) {
        env->SetByteArrayRegion(result, 0, value.size(),
                                reinterpret_cast<const jbyte *>(value.data()));
        checkJava(env);
    }
    return result;
}

std::vector<Bytes> arrays(JNIEnv *env, jobject value) {
    checkJava(env);
    if (!value) throw std::runtime_error("Missing Satochip callback result");
    auto values = static_cast<jobjectArray>(value);
    if (env->GetArrayLength(values) != 2) {
        throw std::runtime_error("Expected two arrays from Satochip callback");
    }
    std::vector<Bytes> result;
    for (int i = 0; i < 2; ++i) {
        auto item = static_cast<jbyteArray>(env->GetObjectArrayElement(values, i));
        result.push_back(bytes(env, item));
        env->DeleteLocalRef(item);
    }
    return result;
}

nunchuk::Nunchuk &sdk() {
    auto &nu = NunchukProvider::get()->nu;
    if (!nu) throw std::runtime_error("Initialize Nunchuk before using Satochip");
    return *nu;
}

class Card {
   public:
    Card(JNIEnv *env, jobject card) : env(env), card(card) {
        if (!card) throw std::invalid_argument("Satochip card is required");
        clazz = env->GetObjectClass(card);
        checkJava(env);
    }
    ~Card() { env->DeleteLocalRef(clazz); }

    jmethodID method(const char *name, const char *signature) {
        auto id = env->GetMethodID(clazz, name, signature);
        checkJava(env);
        return id;
    }

    nunchuk::CardBip32GetExtendedKeyFn key() {
        return [this](const std::string &path) {
            LocalFrame frame(env);
            auto id = method("getExtendedKey", "(Ljava/lang/String;)[[B");
            auto arg = env->NewStringUTF(path.c_str());
            checkJava(env);
            return arrays(env, env->CallObjectMethod(card, id, arg));
        };
    }

    std::function<bool(int)> progress() {
        return [this](int percent) {
            LocalFrame frame(env);
            auto id = method("onProgress", "(I)Z");
            auto keepGoing = env->CallBooleanMethod(card, id, percent);
            checkJava(env);
            return keepGoing == JNI_TRUE;
        };
    }

    void importSeed(const Bytes &seed) {
        LocalFrame frame(env);
        auto id = method("importSeed", "([B)V");
        auto value = array(env, seed);
        env->CallVoidMethod(card, id, value);
        auto error = env->ExceptionOccurred();
        if (error) env->ExceptionClear();
        Bytes zero(seed.size(), 0);
        env->SetByteArrayRegion(value, 0, zero.size(),
                                reinterpret_cast<const jbyte *>(zero.data()));
        if (error) env->Throw(error);
        checkJava(env);
    }

    nunchuk::SatochipSignPsbtParams params() {
        nunchuk::SatochipSignPsbtParams p;
        p.cardBip32GetExtendedKeyFn = key();
        p.cardSignTransactionHashFn = [this](unsigned char key, const Bytes &hash,
                                             const std::optional<Bytes> &) {
            LocalFrame frame(env);
            auto id = method("signTransactionHash", "(I[B)[B");
            auto h = array(env, hash);
            return bytes(env, static_cast<jbyteArray>(
                                  env->CallObjectMethod(card, id, static_cast<jint>(key), h)));
        };
        p.cardTaprootTweakPrivateKeyFn = [this](int key, const Bytes &tweak, bool bypass) {
            LocalFrame frame(env);
            auto id = method("tweakPrivateKey", "(I[BZ)[B");
            auto t = array(env, tweak);
            return bytes(env, static_cast<jbyteArray>(env->CallObjectMethod(
                                  card, id, key, t, static_cast<jboolean>(bypass))));
        };
        p.cardSignSchnorrHashFn = [this](const Bytes &hash, const std::optional<Bytes> &) {
            LocalFrame frame(env);
            auto id = method("signSchnorrHash", "([B)[B");
            auto h = array(env, hash);
            return bytes(env, static_cast<jbyteArray>(env->CallObjectMethod(card, id, h)));
        };
        p.cardMusig2GenerateNonceFn = [this](int key, const Bytes &agg, const Bytes &hash,
                                             const Bytes &extra) {
            LocalFrame frame(env);
            auto id = method("generateNonce", "(I[B[B[B)[[B");
            auto a = array(env, agg);
            auto h = array(env, hash);
            auto e = array(env, extra);
            return arrays(env, env->CallObjectMethod(card, id, key, a, h, e));
        };
        p.cardMusig2SignFn = [this](int key, const Bytes &nonce, const Bytes &b, const Bytes &ea,
                                    bool evenY, bool positiveKey) {
            LocalFrame frame(env);
            auto id = method("signMusig2", "(I[B[B[BZZ)[B");
            auto n = array(env, nonce);
            auto bv = array(env, b);
            auto e = array(env, ea);
            return bytes(env, static_cast<jbyteArray>(env->CallObjectMethod(
                                  card, id, key, n, bv, e, static_cast<jboolean>(evenY),
                                  static_cast<jboolean>(positiveKey))));
        };
        return p;
    }

   private:
    JNIEnv *env;
    jobject card;
    jclass clazz;
};

class Secret {
   public:
    Secret(JNIEnv *env, jbyteArray source) {
        auto raw = bytes(env, source);
        value.assign(raw.begin(), raw.end());
        OPENSSL_cleanse(raw.data(), raw.size());
    }
    ~Secret() { OPENSSL_cleanse(value.data(), value.size()); }
    std::string value;
};

template <class F>
auto invoke(JNIEnv *env, F fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const JavaException &) {
    } catch (nunchuk::BaseException &e) {
        if (!env->ExceptionCheck()) Deserializer::convert2JException(env, e);
    } catch (std::exception &e) {
        if (!env->ExceptionCheck()) Deserializer::convertStdException2JException(env, e);
    }
    return {};
}
}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipGetMasterFingerprint(JNIEnv *env,
                                                                                  jobject,
                                                                                  jobject card) {
    return invoke(env, [&] {
        Card c(env, card);
        auto result = nunchuk::SatochipGetMasterFingerprint(c.key());
        return env->NewStringUTF(result.c_str());
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipAddMasterSigner(JNIEnv *env, jobject,
                                                                             jstring fingerprint,
                                                                             jstring name) {
    invoke(env, [&] {
        sdk().AddSatochip(StringWrapper(env, fingerprint), StringWrapper(env, name));
        return true;
    });
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipCreateMasterSigner(JNIEnv *env,
                                                                                jobject,
                                                                                jobject card,
                                                                                jstring name) {
    return invoke(env, [&] {
        Card c(env, card);
        auto signer =
            sdk().CreateSatochipMasterSigner(c.key(), StringWrapper(env, name), c.progress());
        return Deserializer::convert2JMasterSigner(env, signer);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipCacheMasterSignerXpubs(
    JNIEnv *env, jobject, jobject card, jstring master_id) {
    invoke(env, [&] {
        Card c(env, card);
        sdk().CacheSatochipMasterSignerXPub(c.key(), StringWrapper(env, master_id), c.progress());
        return true;
    });
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipGetSigner(JNIEnv *env, jobject,
                                                                       jobject card,
                                                                       jstring master_id,
                                                                       jstring path) {
    return invoke(env, [&] {
        Card c(env, card);
        auto signer = sdk().GetSignerFromSatochipMasterSigner(
            c.key(), StringWrapper(env, master_id), StringWrapper(env, path));
        return Deserializer::convert2JSigner(env, signer);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipImportSeed(JNIEnv *env, jobject,
                                                                        jobject card,
                                                                        jbyteArray mnemonic,
                                                                        jbyteArray passphrase) {
    invoke(env, [&] {
        Card c(env, card);
        Secret words(env, mnemonic), password(env, passphrase);
        nunchuk::SatochipImportSeedFromMnemonic([&](const Bytes &seed) { c.importSeed(seed); },
                                                words.value, password.value);
        return true;
    });
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipSignMessage(
    JNIEnv *env, jobject, jobject card, jobject signer, jbyteArray message) {
    return invoke(env, [&] {
        Card c(env, card);
        auto params = c.params();
        auto text = bytes(env, message);
        auto result = sdk().SignSatochipMessage(
            params.cardBip32GetExtendedKeyFn, params.cardSignTransactionHashFn,
            Serializer::convert2CSigner(env, signer), std::string(text.begin(), text.end()));
        return env->NewStringUTF(result.c_str());
    });
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipSignPsbt(JNIEnv *env, jobject,
                                                                      jobject card, jobject wallet,
                                                                      jstring psbt) {
    return invoke(env, [&] {
        Card c(env, card);
        auto result = sdk().SignSatochipTransaction(
            c.params(), Serializer::convert2CWallet(env, wallet), StringWrapper(env, psbt));
        return env->NewStringUTF(result.c_str());
    });
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_nunchuk_android_nativelib_LibNunchukAndroid_satochipSignPsbtContent(
    JNIEnv *env, jobject, jobject card, jstring content, jstring name, jstring psbt) {
    return invoke(env, [&] {
        Card c(env, card);
        auto wallet = nunchuk::Utils::ParseWalletDescriptor(StringWrapper(env, content));
        wallet.set_name(StringWrapper(env, name));
        auto result = sdk().SignSatochipTransaction(c.params(), wallet, StringWrapper(env, psbt));
        return env->NewStringUTF(result.c_str());
    });
}
