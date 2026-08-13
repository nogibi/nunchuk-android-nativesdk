package com.nunchuk.android.bitbox

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.model.Wallet
import com.nunchuk.android.model.bridge.toBridge
import com.nunchuk.android.nativelib.LibNunchukAndroid

/** Requires [com.nunchuk.android.nativelib.NunchukNativeSdk.initNunchuk] first. */
class BitBoxNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(
        sessionId: String,
        authenticatedBond: Boolean,
        reportsPerWrite: Int = 1,
    ) = native.bitBoxCreateSession(sessionId, authenticatedBond, reportsPerWrite)

    @Throws(NCNativeException::class)
    fun initialize(sessionId: String): BitBoxStep = native.bitBoxInitialize(sessionId)

    @Throws(NCNativeException::class)
    fun reconnect(
        sessionId: String,
        authenticatedBond: Boolean,
        reportsPerWrite: Int = 1,
    ): BitBoxStep = native.bitBoxReconnect(sessionId, authenticatedBond, reportsPerWrite)

    @Throws(NCNativeException::class)
    fun onDisconnected(sessionId: String) = native.bitBoxOnDisconnected(sessionId)

    @Throws(NCNativeException::class)
    fun removeSession(sessionId: String) = native.bitBoxRemoveSession(sessionId)

    @Throws(NCNativeException::class)
    fun confirmPairing(sessionId: String, accepted: Boolean): BitBoxStep =
        native.bitBoxConfirmPairing(sessionId, accepted)

    @Throws(NCNativeException::class)
    fun cancel(sessionId: String): BitBoxStep = native.bitBoxCancel(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(
        sessionId: String,
        derivationPath: String,
        checkOnDevice: Boolean = false,
    ): BitBoxStep = native.bitBoxGetExtendedPublicKey(
        sessionId,
        derivationPath,
        checkOnDevice,
    )

    @Throws(NCNativeException::class)
    fun getMasterFingerprint(sessionId: String): BitBoxStep =
        native.bitBoxGetMasterFingerprint(sessionId)

    @Throws(NCNativeException::class)
    fun isWalletRegistered(sessionId: String, wallet: Wallet): BitBoxStep =
        native.bitBoxIsWalletRegistered(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun registerWallet(sessionId: String, wallet: Wallet): BitBoxStep =
        native.bitBoxRegisterWallet(sessionId, wallet.toBridge())

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: Wallet,
        addressIndex: Int,
        checkOnDevice: Boolean = true,
        change: Boolean = false,
    ): BitBoxStep = native.bitBoxGetWalletAddress(
        sessionId,
        wallet.toBridge(),
        addressIndex,
        checkOnDevice,
        change,
    )

    @Throws(NCNativeException::class)
    fun signMessage(
        sessionId: String,
        derivationPath: String,
        message: String,
    ): BitBoxStep = native.bitBoxSignMessage(sessionId, derivationPath, message)

    @Throws(NCNativeException::class)
    fun signPsbt(sessionId: String, wallet: Wallet, psbt: String): BitBoxStep =
        native.bitBoxSignPsbt(sessionId, wallet.toBridge(), psbt)

    @Throws(NCNativeException::class)
    fun resume(sessionId: String): BitBoxStep = native.bitBoxResume(sessionId)

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): BitBoxStep =
        native.bitBoxOnData(sessionId, data)

    @Throws(NCNativeException::class)
    fun isInitialized(sessionId: String): Boolean = native.bitBoxIsInitialized(sessionId)

    @Throws(NCNativeException::class)
    fun getDeviceInfo(sessionId: String): BitBoxDeviceInfo =
        native.bitBoxGetDeviceInfo(sessionId)

    @Throws(NCNativeException::class)
    fun getInitializeResult(sessionId: String): BitBoxInitializeResult =
        native.bitBoxGetInitializeResult(sessionId)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String =
        native.bitBoxGetExtendedPublicKeyResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMasterFingerprintResult(sessionId: String): String =
        native.bitBoxGetMasterFingerprintResult(sessionId)

    @Throws(NCNativeException::class)
    fun getRegistrationResult(sessionId: String): Boolean =
        native.bitBoxGetRegistrationResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String =
        native.bitBoxGetWalletAddressResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMessageSignatureResult(sessionId: String): String =
        native.bitBoxGetMessageSignatureResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSignPsbtResult(sessionId: String): String =
        native.bitBoxGetSignPsbtResult(sessionId)
}
