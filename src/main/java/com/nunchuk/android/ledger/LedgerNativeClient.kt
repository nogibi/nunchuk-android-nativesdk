package com.nunchuk.android.ledger

import com.nunchuk.android.exception.NCNativeException
import com.nunchuk.android.nativelib.LibNunchukAndroid

class LedgerNativeClient {
    private val native = LibNunchukAndroid()

    @Throws(NCNativeException::class)
    fun createSession(sessionId: String, config: LedgerSessionConfig) {
        native.ledgerCreateSession(sessionId, config)
    }

    @Throws(NCNativeException::class)
    fun reset(sessionId: String) {
        native.ledgerReset(sessionId)
    }

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
    fun getMasterFingerprint(sessionId: String): LedgerStep {
        return native.ledgerGetMasterFingerprint(sessionId)
    }

    @Throws(NCNativeException::class)
    fun signMessage(
        sessionId: String,
        derivationPath: String,
        message: String,
    ): LedgerStep {
        return native.ledgerSignMessage(sessionId, derivationPath, message)
    }

    @Throws(NCNativeException::class)
    fun registerWallet(
        sessionId: String,
        policy: LedgerWalletPolicy,
    ): LedgerStep {
        return native.ledgerRegisterWallet(sessionId, policy)
    }

    @Throws(NCNativeException::class)
    fun getWalletAddress(
        sessionId: String,
        wallet: LedgerRegisteredWallet,
        addressIndex: Int,
        checkOnDevice: Boolean = false,
        change: Boolean = false,
    ): LedgerStep {
        return native.ledgerGetWalletAddress(
            sessionId,
            wallet,
            addressIndex,
            checkOnDevice,
            change,
        )
    }

    @Throws(NCNativeException::class)
    fun resume(sessionId: String): LedgerStep {
        return native.ledgerResume(sessionId)
    }

    @Throws(NCNativeException::class)
    fun onData(sessionId: String, data: ByteArray): LedgerStep {
        return native.ledgerOnData(sessionId, data)
    }

    @Throws(NCNativeException::class)
    fun getExtendedPublicKeyResult(sessionId: String): String {
        return native.ledgerGetExtendedPublicKeyResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getMasterFingerprintResult(sessionId: String): ByteArray {
        return native.ledgerGetMasterFingerprintResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getMasterFingerprintStringResult(sessionId: String): String {
        return native.ledgerGetMasterFingerprintStringResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getMessageSignatureResult(sessionId: String): LedgerMessageSignature {
        return native.ledgerGetMessageSignatureResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getMessageSignatureStringResult(sessionId: String): String {
        return native.ledgerGetMessageSignatureStringResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getRegisteredWalletResult(sessionId: String): LedgerRegisteredWallet {
        return native.ledgerGetRegisteredWalletResult(sessionId)
    }

    @Throws(NCNativeException::class)
    fun getWalletAddressResult(sessionId: String): String {
        return native.ledgerGetWalletAddressResult(sessionId)
    }
}
