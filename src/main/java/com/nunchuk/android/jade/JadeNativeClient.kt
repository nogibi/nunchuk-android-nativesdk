package com.nunchuk.android.jade

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.model.Wallet
import com.nunchuk.android.model.bridge.toBridge
import com.nunchuk.android.nativelib.LibNunchukAndroid

/**
 * Requires [com.nunchuk.android.nativelib.NunchukNativeSdk.initNunchuk] first.
 * Call [createSession], [onData], and [confirmCustomPinServer] off the UI thread
 * because Jade PIN-server HTTP is handled synchronously inside libnunchuk.
 */
class JadeNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(sessionId: String, maxWriteSize: Int): JadeStep =
        native.jadeCreateSession(sessionId, maxWriteSize)

    @Throws(NCNativeException::class)
    fun confirmCustomPinServer(sessionId: String, accepted: Boolean): JadeStep =
        native.jadeConfirmCustomPinServer(sessionId, accepted)

    @Throws(NCNativeException::class)
    fun getVersionInfo(sessionId: String): JadeStep = native.jadeGetVersionInfo(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(sessionId: String, derivationPath: String): JadeStep =
        native.jadeGetExtendedPublicKey(sessionId, derivationPath)

    @Throws(NCNativeException::class)
    fun getMasterFingerprint(sessionId: String): JadeStep =
        native.jadeGetMasterFingerprint(sessionId)

    @Throws(NCNativeException::class)
    fun isWalletRegistered(sessionId: String, wallet: Wallet): JadeStep =
        native.jadeIsWalletRegistered(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun isWalletRegistered(
        sessionId: String,
        walletContent: String,
        walletName: String,
    ): JadeStep = native.jadeIsWalletRegisteredContent(sessionId, walletContent, walletName)

    @Throws(NCNativeException::class)
    fun registerWallet(sessionId: String, wallet: Wallet): JadeStep =
        native.jadeRegisterWallet(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun registerWallet(
        sessionId: String,
        walletContent: String,
        walletName: String,
    ): JadeStep = native.jadeRegisterWalletContent(sessionId, walletContent, walletName)

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: Wallet,
        addressIndex: Int,
        change: Boolean = false,
    ): JadeStep = native.jadeGetWalletAddress(
        sessionId,
        wallet.toBridge(),
        addressIndex,
        change,
    )

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        walletContent: String,
        walletName: String,
        addressIndex: Int,
        change: Boolean = false,
    ): JadeStep = native.jadeGetWalletAddressContent(
        sessionId,
        walletContent,
        walletName,
        addressIndex,
        change,
    )

    @Throws(NCNativeException::class)
    fun signMessage(
        sessionId: String,
        derivationPath: String,
        message: String,
    ): JadeStep = native.jadeSignMessage(sessionId, derivationPath, message)

    @Throws(NCNativeException::class)
    fun signPsbt(sessionId: String, wallet: Wallet, psbt: String): JadeStep =
        native.jadeSignPsbt(sessionId, wallet.toBridge(), psbt)

    @Throws(NCNativeException::class)
    fun signPsbt(
        sessionId: String,
        walletContent: String,
        walletName: String,
        psbt: String,
    ): JadeStep = native.jadeSignPsbtContent(sessionId, walletContent, walletName, psbt)

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): JadeStep = native.jadeOnData(sessionId, data)

    @Throws(NCNativeException::class)
    fun isInitialized(sessionId: String): Boolean = native.jadeIsInitialized(sessionId)

    @Throws(NCNativeException::class)
    fun getDeviceInfo(sessionId: String): JadeDeviceInfo = native.jadeGetDeviceInfo(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String =
        native.jadeGetExtendedPublicKeyResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMasterFingerprintResult(sessionId: String): String =
        native.jadeGetMasterFingerprintResult(sessionId)

    @Throws(NCNativeException::class)
    fun getRegistrationResult(sessionId: String): Boolean =
        native.jadeGetRegistrationResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String =
        native.jadeGetWalletAddressResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMessageSignatureResult(sessionId: String): String =
        native.jadeGetMessageSignatureResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSignPsbtResult(sessionId: String): String = native.jadeGetSignPsbtResult(sessionId)
}
