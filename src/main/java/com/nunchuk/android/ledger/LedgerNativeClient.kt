package com.nunchuk.android.ledger

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.nativelib.LibNunchukAndroid

class LedgerNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(sessionId: String, transport: LedgerTransport) =
        native.ledgerCreateSession(sessionId, transport)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKey(
        sessionId: String,
        derivationPath: String,
        checkOnDevice: Boolean = false,
    ): LedgerStep = native.ledgerGetExtendedPublicKey(
        sessionId = sessionId,
        derivationPath = derivationPath,
        checkOnDevice = checkOnDevice,
    )

    @Throws(NCNativeException::class)
    fun getMasterFingerprint(sessionId: String): LedgerStep =
        native.ledgerGetMasterFingerprint(sessionId)

    @Throws(NCNativeException::class)
    fun signMessage(
        sessionId: String,
        derivationPath: String,
        message: String,
    ): LedgerStep = native.ledgerSignMessage(sessionId, derivationPath, message)

    @Throws(NCNativeException::class)
    fun registerWallet(
        sessionId: String,
        policy: LedgerWalletPolicy,
    ): LedgerStep = native.ledgerRegisterWallet(sessionId, policy)

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: LedgerRegisteredWallet,
        addressIndex: Int,
        checkOnDevice: Boolean = false,
        change: Boolean = false,
    ): LedgerStep = native.ledgerGetWalletAddress(
        sessionId,
        wallet,
        addressIndex,
        checkOnDevice,
        change,
    )

    @Throws(NCNativeException::class)
    fun signPsbt(
        sessionId: String,
        psbt: String,
        wallet: LedgerRegisteredWallet,
    ): LedgerStep = native.ledgerSignPsbt(sessionId, psbt, wallet)

    @Throws(NCNativeException::class)
    fun resume(sessionId: String): LedgerStep = native.ledgerResume(sessionId)

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): LedgerStep =
        native.ledgerOnData(sessionId, data)

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String =
        native.ledgerGetExtendedPublicKeyResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMasterFingerprintStringResult(sessionId: String): String =
        native.ledgerGetMasterFingerprintStringResult(sessionId)

    @Throws(NCNativeException::class)
    fun getMessageSignatureStringResult(sessionId: String): String =
        native.ledgerGetMessageSignatureStringResult(sessionId)

    @Throws(NCNativeException::class)
    fun getRegisteredWalletResult(sessionId: String): LedgerRegisteredWalletResult =
        native.ledgerGetRegisteredWalletResult(sessionId)

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String =
        native.ledgerGetWalletAddressResult(sessionId)

    @Throws(NCNativeException::class)
    fun getSignPsbtStringResult(sessionId: String): String =
        native.ledgerGetSignPsbtStringResult(sessionId)
}
