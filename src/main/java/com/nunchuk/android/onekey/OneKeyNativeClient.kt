package com.nunchuk.android.onekey

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.model.Wallet
import com.nunchuk.android.model.bridge.toBridge
import com.nunchuk.android.nativelib.LibNunchukAndroid

/**
 * Initialize Nunchuk first. Serialize calls for each connection and drive every returned step.
 * After failure, timeout or disconnect, close the transport and create a fresh session.
 * Pass raw USB reports/BLE notifications to onData; never log PIN/passphrase writes.
 */
class OneKeyNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(sessionId: String, transport: OneKeyTransport): OneKeyStep =
        native.oneKeyCreateSession(sessionId, transport)

    @Throws(NCNativeException::class)
    fun getMasterFingerprint(sessionId: String): OneKeyStep =
        native.oneKeyGetMasterFingerprint(sessionId)

    @Throws(NCNativeException::class)
    fun enterPinOnDevice(sessionId: String): OneKeyStep =
        native.oneKeyEnterPinOnDevice(sessionId)

    @Throws(NCNativeException::class)
    fun cancel(sessionId: String): OneKeyStep =
        native.oneKeyCancel(sessionId)

    @Throws(NCNativeException::class)
    fun onDisconnected(sessionId: String): OneKeyStep =
        native.oneKeyOnDisconnected(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(
        sessionId: String,
        derivationPath: String,
        checkOnDevice: Boolean = false,
    ): OneKeyStep =
        native.oneKeyGetExtendedPublicKey(sessionId, derivationPath, checkOnDevice)

    @Throws(NCNativeException::class)
    fun providePin(sessionId: String, pin: String): OneKeyStep =
        native.oneKeyProvidePin(sessionId, pin)

    @Throws(NCNativeException::class)
    fun enterPassphraseOnDevice(sessionId: String, attachToPin: Boolean = false): OneKeyStep =
        native.oneKeyEnterPassphraseOnDevice(sessionId, attachToPin)

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): OneKeyStep =
        native.oneKeyOnData(sessionId, data)

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: Wallet,
        addressIndex: Int,
        checkOnDevice: Boolean = true,
        change: Boolean = false,
    ): OneKeyStep =
        native.oneKeyGetWalletAddress(sessionId, wallet.toBridge(), addressIndex, checkOnDevice, change)

    @Throws(NCNativeException::class)
    fun signPsbt(sessionId: String, wallet: Wallet, psbt: String): OneKeyStep =
        native.oneKeySignPsbt(sessionId, wallet.toBridge(), psbt)

    @Throws(NCNativeException::class)
    fun isInitialized(sessionId: String): Boolean =
        native.oneKeyIsInitialized(sessionId)

    @Throws(NCNativeException::class)
    fun getDeviceInfo(sessionId: String): OneKeyDeviceInfo =
        native.oneKeyGetDeviceInfo(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String =
        native.oneKeyGetExtendedPublicKeyResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMasterFingerprintResult(sessionId: String): String =
        native.oneKeyGetMasterFingerprintResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String =
        native.oneKeyGetWalletAddressResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMessageSignatureResult(sessionId: String): String =
        native.oneKeyGetMessageSignatureResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSignPsbtResult(sessionId: String): String =
        native.oneKeyGetSignPsbtResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        walletContent: String,
        walletName: String,
        addressIndex: Int,
        checkOnDevice: Boolean = true,
        change: Boolean = false,
    ): OneKeyStep =
        native.oneKeyGetWalletAddressContent(sessionId, walletContent, walletName, addressIndex, checkOnDevice, change)

    @Throws(NCNativeException::class)
    fun signPsbt(
        sessionId: String,
        walletContent: String,
        walletName: String,
        psbt: String,
    ): OneKeyStep =
        native.oneKeySignPsbtContent(sessionId, walletContent, walletName, psbt)

    @Throws(NCNativeException::class)
    fun signMessage(sessionId: String, derivationPath: String, message: String): OneKeyStep =
        native.oneKeySignMessage(sessionId, derivationPath, message.toByteArray(Charsets.UTF_8))

    @Throws(NCNativeException::class)
    fun providePassphrase(sessionId: String, passphrase: String): OneKeyStep {
        val bytes = passphrase.toByteArray(Charsets.UTF_8)
        return try {
            native.oneKeyProvidePassphrase(sessionId, bytes)
        } finally {
            bytes.fill(0)
        }
    }
}
